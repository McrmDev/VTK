// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) 2010,2015 David Gobbi
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkParseMerge.h"
#include "vtkParse.h"
#include "vtkParseData.h"
#include "vtkParseExtras.h"
#include "vtkParseMain.h"
#include "vtkParseSystem.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* add a class to the MergeInfo */
int vtkParseMerge_PushClass(MergeInfo* info, const char* classname)
{
  int n = info->NumberOfClasses;
  int m = 0;
  int i;
  const char** classnames;
  char* cp;

  /* if class is already there, return its index */
  for (i = 0; i < n; i++)
  {
    if (strcmp(info->ClassNames[i], classname) == 0)
    {
      return i;
    }
  }

  /* if no elements yet, reserve four slots */
  if (n == 0)
  {
    m = 4;
  }
  /* else double the slots whenever size is a power of two */
  if (n >= 4 && (n & (n - 1)) == 0)
  {
    m = (n << 1);
  }

  if (m)
  {
    classnames = (const char**)malloc(m * sizeof(const char*));
    if (n)
    {
      for (i = 0; i < n; i++)
      {
        classnames[i] = info->ClassNames[i];
      }
      free((char**)info->ClassNames);
    }
    info->ClassNames = classnames;
  }

  info->NumberOfClasses = n + 1;
  cp = (char*)malloc(strlen(classname) + 1);
  strcpy(cp, classname);
  info->ClassNames[n] = cp;

  return n;
}

/* add a function to the MergeInfo */
int vtkParseMerge_PushFunction(MergeInfo* info, int depth)
{
  int n = info->NumberOfFunctions;
  int m = 0;
  int i;
  int* overrides;
  int** classes;

  /* if no elements yet, reserve four slots */
  if (n == 0)
  {
    m = 4;
  }
  /* else double the slots whenever size is a power of two */
  else if (n >= 4 && (n & (n - 1)) == 0)
  {
    m = (n << 1);
  }

  if (m)
  {
    overrides = (int*)malloc(m * sizeof(int));
    classes = (int**)malloc(m * sizeof(int*));
    if (n)
    {
      for (i = 0; i < n; i++)
      {
        overrides[i] = info->NumberOfOverrides[i];
        classes[i] = info->OverrideClasses[i];
      }
      free(info->NumberOfOverrides);
      free(info->OverrideClasses);
    }
    info->NumberOfOverrides = overrides;
    info->OverrideClasses = classes;
  }

  info->NumberOfFunctions = n + 1;
  info->NumberOfOverrides[n] = 1;
  info->OverrideClasses[n] = (int*)malloc(sizeof(int));
  info->OverrideClasses[n][0] = depth;

  return n;
}

/* add an override to to the specified function */
int vtkParseMerge_PushOverride(MergeInfo* info, int i, int depth)
{
  int n = info->NumberOfOverrides[i];
  int m = 0;
  int j;
  int* classes;

  /* Make sure it hasn't already been pushed */
  for (j = 0; j < info->NumberOfOverrides[i]; j++)
  {
    if (info->OverrideClasses[i][j] == depth)
    {
      return i;
    }
  }

  /* if n is a power of two */
  if ((n & (n - 1)) == 0)
  {
    m = (n << 1);
    classes = (int*)malloc(m * sizeof(int));
    for (j = 0; j < n; j++)
    {
      classes[j] = info->OverrideClasses[i][j];
    }
    free(info->OverrideClasses[i]);
    info->OverrideClasses[i] = classes;
  }

  info->NumberOfOverrides[i] = n + 1;
  info->OverrideClasses[i][n] = depth;

  return n;
}

/* return an initialized MergeInfo */
MergeInfo* vtkParseMerge_CreateMergeInfo(const ClassInfo* classInfo)
{
  int i, n;
  MergeInfo* info = (MergeInfo*)malloc(sizeof(MergeInfo));
  info->NumberOfClasses = 0;
  info->NumberOfFunctions = 0;

  vtkParseMerge_PushClass(info, classInfo->Name);
  n = classInfo->NumberOfFunctions;
  for (i = 0; i < n; i++)
  {
    vtkParseMerge_PushFunction(info, 0);
  }

  return info;
}

/* free the MergeInfo */
void vtkParseMerge_FreeMergeInfo(MergeInfo* info)
{
  int i, n;

  n = info->NumberOfClasses;
  for (i = 0; i < n; i++)
  {
    free((char*)info->ClassNames[i]);
  }
  free((char**)info->ClassNames);

  n = info->NumberOfFunctions;
  for (i = 0; i < n; i++)
  {
    free(info->OverrideClasses[i]);
  }
  if (n)
  {
    free(info->NumberOfOverrides);
    free(info->OverrideClasses);
  }

  free(info);
}

/* merge a function */
static void merge_function(FileInfo* finfo, FunctionInfo* merge, const FunctionInfo* func)
{
  int i, j;

  /* virtuality is inherited */
  if (func->IsVirtual)
  {
    merge->IsVirtual = 1;
  }

  /* contracts are inherited */
  if (merge->NumberOfPreconds == 0)
  {
    for (i = 0; i < func->NumberOfPreconds; i++)
    {
      StringTokenizer t;
      int qualified = 0;
      char text[512];
      size_t l = 0;

      /* tokenize the contract code according to C/C++ rules */
      vtkParse_InitTokenizer(&t, func->Preconds[i], WS_DEFAULT);
      do
      {
        int matched = 0;

        /* check for unqualified identifiers */
        if (t.tok == TOK_ID && !qualified)
        {
          /* check if the unqualified identifier is a parameter name */
          for (j = 0; j < func->NumberOfParameters; j++)
          {
            const ValueInfo* arg = func->Parameters[j];
            const char* name = arg->Name;
            if (name && strlen(name) == t.len && strncmp(name, t.text, t.len) == 0)
            {
              matched = 1;
              name = merge->Parameters[j]->Name;
              if (name)
              {
                /* change it to the new parameter name */
                l += snprintf(&text[l], sizeof(text) - l, "%s", name);
              }
              else
              {
                /* parameter has no name, use a number */
                l += snprintf(&text[l], sizeof(text) - l, "(#%d)", j);
              }
              break;
            }
          }
        }

        if (!matched)
        {
          strncpy(&text[l], t.text, t.len);
          l += t.len;
        }

        /* if next character is whitespace, add a space */
        if (vtkParse_CharType(t.text[t.len], CPRE_WHITE))
        {
          text[l++] = ' ';
        }

        /* check whether the next identifier is qualified */
        qualified = (t.tok == TOK_SCOPE || t.tok == TOK_ARROW || t.tok == '.');
      } while (vtkParse_NextToken(&t));

      vtkParse_AddStringToArray(
        &merge->Preconds, &merge->NumberOfPreconds, vtkParse_CacheString(finfo->Strings, text, l));
    }
  }

  /* hints are inherited */
  j = func->NumberOfParameters;
  for (i = -1; i < j; i++)
  {
    ValueInfo* arg = merge->ReturnValue;
    const ValueInfo* arg2 = func->ReturnValue;
    if (i >= 0)
    {
      arg = merge->Parameters[i];
      arg2 = func->Parameters[i];
    }
    if (arg && arg2)
    {
      if (arg2->CountHint && !arg->CountHint)
      {
        arg->CountHint = arg2->CountHint;
      }
      else if (arg2->Count && !arg->Count)
      {
        arg->Count = arg2->Count;
      }
      /* attribute flags */
      arg->Attributes |= arg2->Attributes;
    }
  }

#ifndef VTK_PARSE_LEGACY_REMOVE
  if (func->HaveHint && !merge->HaveHint)
  {
    merge->HaveHint = func->HaveHint;
    merge->HintSize = func->HintSize;
  }
#endif

  /* comments are inherited */
  if (func->Comment && !merge->Comment)
  {
    merge->Comment = func->Comment;
  }
}

/* try to resolve "Using" declarations with the given class. */
void vtkParseMerge_MergeUsing(
  FileInfo* finfo, MergeInfo* info, ClassInfo* merge, const ClassInfo* super, int depth)
{
  int i, j, k, ii, n, m;
  char* cp;
  size_t l;
  int match;
  UsingInfo* u;
  UsingInfo* v;
  FunctionInfo* func;
  FunctionInfo* f2;
  ValueInfo* param;
  const char* lastval;
  int is_constructor;

  /* if scope matches, rename scope to "Superclass", */
  /* this will cause any inherited scopes to match */
  match = 0;
  for (ii = 0; ii < merge->NumberOfUsings; ii++)
  {
    u = merge->Usings[ii];
    if (u->Scope)
    {
      match = 1;
      if (strcmp(u->Scope, super->Name) == 0)
      {
        u->Scope = "Superclass";
      }
    }
  }
  if (!match)
  {
    /* nothing to do! */
    return;
  }

  m = merge->NumberOfFunctions;
  n = super->NumberOfFunctions;
  for (i = 0; i < n; i++)
  {
    func = super->Functions[i];

    if (!func->Name)
    {
      continue;
    }

    /* destructors cannot be used */
    if (func->Name[0] == '~' && strcmp(&func->Name[1], super->Name) == 0)
    {
      continue;
    }

    /* constructors can be used, with limitations */
    is_constructor = 0;
    if (strcmp(func->Name, super->Name) == 0)
    {
      is_constructor = 1;
      if (func->Template)
      {
        /* templated constructors cannot be "used" */
        continue;
      }
    }

    /* check that the function is being "used" */
    u = NULL;
    for (ii = 0; ii < merge->NumberOfUsings; ii++)
    {
      v = merge->Usings[ii];
      if (v->Scope && strcmp(v->Scope, "Superclass") == 0)
      {
        if (v->Name && strcmp(v->Name, func->Name) == 0)
        {
          u = v;
          break;
        }
      }
    }
    if (!u)
    {
      continue;
    }

    /* look for override of this signature */
    match = 0;
    for (j = 0; j < m; j++)
    {
      f2 = merge->Functions[j];
      if (f2->Name &&
        ((is_constructor && strcmp(f2->Name, merge->Name) == 0) ||
          (!is_constructor && strcmp(f2->Name, func->Name) == 0)))
      {
        if (vtkParse_CompareFunctionSignature(func, f2) != 0)
        {
          match = 1;
          break;
        }
      }
    }
    if (!match)
    {
      /* copy into the merge */
      if (is_constructor)
      {
        /* constructors require special default argument handling, there
         * is a different used constructor for each arg with a default */
        for (j = func->NumberOfParameters; j > 0; j--)
        {
          param = func->Parameters[0];
          if (j == 1 && param->Class && strcmp(param->Class, super->Name) == 0 &&
            (param->Type & VTK_PARSE_POINTER_MASK) == 0)
          {
            /* it is a copy constructor, it will not be "used" */
            continue;
          }
          f2 = (FunctionInfo*)malloc(sizeof(FunctionInfo));
          vtkParse_InitFunction(f2);
          f2->Access = u->Access;
          f2->Name = merge->Name;
          f2->Class = merge->Name;
          f2->Comment = func->Comment;
          f2->IsExplicit = func->IsExplicit;
          l = vtkParse_FunctionInfoToString(f2, NULL, VTK_PARSE_EVERYTHING);
          cp = vtkParse_NewString(finfo->Strings, l);
          vtkParse_FunctionInfoToString(f2, cp, VTK_PARSE_EVERYTHING);
          f2->Signature = cp;
          lastval = NULL;
          for (k = 0; k < j; k++)
          {
            param = (ValueInfo*)malloc(sizeof(ValueInfo));
            vtkParse_CopyValue(param, func->Parameters[k]);
            lastval = param->Value;
            param->Value = NULL; /* clear default parameter value */
            vtkParse_AddParameterToFunction(f2, param);
          }
          vtkParse_AddFunctionToClass(merge, f2);
          if (info)
          {
            vtkParseMerge_PushFunction(info, depth);
          }
          if (lastval == NULL)
          {
            /* continue if last parameter had a default value */
            break;
          }
        }
      }
      else
      {
        /* non-constructor methods are simple */
        f2 = (FunctionInfo*)malloc(sizeof(FunctionInfo));
        vtkParse_CopyFunction(f2, func);
        f2->Access = u->Access;
        f2->Class = merge->Name;
        vtkParse_AddFunctionToClass(merge, f2);
        if (info)
        {
          vtkParseMerge_PushFunction(info, depth);
        }
      }
    }
  }

  /* remove any using declarations that were satisfied */
  for (i = 0; i < merge->NumberOfUsings; i++)
  {
    u = merge->Usings[i];
    if (u->Scope && strcmp(u->Scope, "Superclass") == 0)
    {
      match = 0;
      for (j = 0; j < super->NumberOfUsings && !match; j++)
      {
        v = super->Usings[j];
        if (v->Name && u->Name && strcmp(u->Name, v->Name) == 0)
        {
          /* get the new scope so that recursion will occur */
          u->Scope = v->Scope;
          match = 1;
        }
      }
      for (j = 0; j < super->NumberOfFunctions && !match; j++)
      {
        func = super->Functions[j];
        if (u->Name && func->Name && strcmp(func->Name, u->Name) == 0)
        {
          /* ignore this "using" from now on */
          merge->Usings[i]->Name = NULL;
          merge->Usings[i]->Scope = NULL;
          match = 1;
        }
      }
    }
  }
}

/* add "super" methods to the merge */
int vtkParseMerge_Merge(FileInfo* finfo, MergeInfo* info, ClassInfo* merge, ClassInfo* super)
{
  int i, j, ii, n, m, depth;
  int match;
  const FunctionInfo* func;
  FunctionInfo* f1;
  FunctionInfo* f2;

  depth = vtkParseMerge_PushClass(info, super->Name);

  vtkParseMerge_MergeUsing(finfo, info, merge, super, depth);

  m = merge->NumberOfFunctions;
  n = super->NumberOfFunctions;
  for (i = 0; i < n; i++)
  {
    func = super->Functions[i];

    if (!func || !func->Name)
    {
      continue;
    }

    /* constructors and destructors are not inherited */
    if ((strcmp(func->Name, super->Name) == 0) ||
      (func->Name[0] == '~' && strcmp(&func->Name[1], super->Name) == 0))
    {
      continue;
    }

    /* check for overridden functions */
    match = 0;
    for (j = 0; j < m; j++)
    {
      f2 = merge->Functions[j];
      if (f2->Name && strcmp(f2->Name, func->Name) == 0)
      {
        match = 1;
        break;
      }
    }

    /* find all superclass methods with this name */
    for (ii = i; ii < n; ii++)
    {
      f1 = super->Functions[ii];
      if (f1 && f1->Name && strcmp(f1->Name, func->Name) == 0)
      {
        if (match)
        {
          /* look for override of this signature */
          for (j = 0; j < m; j++)
          {
            f2 = merge->Functions[j];
            if (f2->Name && strcmp(f2->Name, f1->Name) == 0)
            {
              if (vtkParse_CompareFunctionSignature(f1, f2) != 0)
              {
                merge_function(finfo, f2, f1);
                vtkParseMerge_PushOverride(info, j, depth);
              }
            }
          }
        }
        else /* no match */
        {
          /* copy into the merge */
          vtkParse_AddFunctionToClass(merge, f1);
          vtkParseMerge_PushFunction(info, depth);
          m++;
          /* remove from future consideration */
          super->Functions[ii] = NULL;
        }
      }
    }
  }

  /* remove all used methods from the superclass */
  j = 0;
  for (i = 0; i < n; i++)
  {
    if (super->Functions[i] != NULL)
    {
      super->Functions[j++] = super->Functions[i];
    }
  }
  if (n && !j)
  {
    free(super->Functions);
    super->Functions = NULL;
  }
  super->NumberOfFunctions = j;

  return depth;
}

/* Recursive suproutine to add the methods of "classname" and all its
 * superclasses to "merge" */
void vtkParseMerge_MergeHelper(FileInfo* finfo, const NamespaceInfo* data,
  const HierarchyInfo* hinfo, const char* classname, int nhintfiles, char** hintfiles,
  MergeInfo* info, ClassInfo* merge)
{
  FILE* fp = NULL;
  ClassInfo* cinfo = NULL;
  ClassInfo* new_cinfo = NULL;
  HierarchyEntry* entry = NULL;
  const char** template_args = NULL;
  int template_arg_count = 0;
  const char* nspacename;
  const char* header;
  const char* filename;
  int i, j, n, m;
  int recurse;
  FILE* hintfile = NULL;
  int ihintfiles = 0;
  const char* hintfilename = NULL;
  FileInfo* new_finfo = NULL;

  /* Note: this method does not deal with scoping yet.
   * "classname" might be a scoped name, in which case the
   * part before the colon indicates the class or namespace
   * (or combination thereof) where the class resides.
   * Each containing namespace or class for the "merge"
   * must be searched, taking the "using" directives that
   * have been applied into account. */

  /* get extra class info from the hierarchy file */
  nspacename = data->Name;
  if (classname[0] == ':' && classname[1] == ':')
  {
    entry = vtkParseHierarchy_FindEntry(hinfo, &classname[2]);
  }
  else
  {
    entry = vtkParseHierarchy_FindEntryEx(hinfo, classname, nspacename);
  }

  if (entry && entry->NumberOfTemplateParameters > 0)
  {
    /* extract the template arguments */
    template_arg_count = entry->NumberOfTemplateParameters;
    vtkParse_DecomposeTemplatedType(
      classname, &classname, template_arg_count, &template_args, entry->TemplateDefaults);
  }

  /* find out if "classname" is in the current namespace */
  n = data->NumberOfClasses;
  for (i = 0; i < n; i++)
  {
    if (strcmp(data->Classes[i]->Name, classname) == 0)
    {
      cinfo = data->Classes[i];
      break;
    }
  }

  if (n > 0 && !cinfo)
  {
    if (!entry)
    {
      return;
    }
    header = entry->HeaderFile;
    if (!header)
    {
      fprintf(stderr, "Null header file for class %s!\n", classname);
      exit(1);
    }

    filename = vtkParse_FindIncludeFile(header);
    if (!filename)
    {
      fprintf(stderr, "Couldn't locate header file %s\n", header);
      exit(1);
    }

    fp = vtkParse_FileOpen(filename, "r");
    if (!fp)
    {
      fprintf(stderr, "Couldn't open header file %s\n", header);
      exit(1);
    }

    new_finfo = vtkParse_ParseFile(filename, fp, stderr);
    fclose(fp);

    if (!new_finfo)
    {
      exit(1);
    }

    if (nhintfiles > 0 && hintfiles)
    {
      for (ihintfiles = 0; ihintfiles < nhintfiles; ihintfiles++)
      {
        hintfilename = hintfiles[ihintfiles];
        if (hintfilename && hintfilename[0] != '\0')
        {
          if (!(hintfile = vtkParse_FileOpen(hintfilename, "r")))
          {
            fprintf(stderr, "Error opening hint file %s\n", hintfilename);
            vtkParse_FreeFile(new_finfo);
            free(new_finfo);
            exit(1);
          }

          vtkParse_ReadHints(new_finfo, hintfile, stderr);
          fclose(hintfile);
        }
      }
    }

    data = new_finfo->Contents;
    if (nspacename)
    {
      m = data->NumberOfNamespaces;
      for (j = 0; j < m; j++)
      {
        NamespaceInfo* ni = data->Namespaces[j];
        if (ni->Name && strcmp(ni->Name, nspacename) == 0)
        {
          n = ni->NumberOfClasses;
          for (i = 0; i < n; i++)
          {
            if (strcmp(ni->Classes[i]->Name, classname) == 0)
            {
              cinfo = ni->Classes[i];
              data = ni;
              break;
            }
          }
          if (i < n)
          {
            break;
          }
        }
      }
    }
    else
    {
      n = data->NumberOfClasses;
      for (i = 0; i < n; i++)
      {
        if (strcmp(data->Classes[i]->Name, classname) == 0)
        {
          cinfo = data->Classes[i];
          break;
        }
      }
    }
  }

  if (cinfo)
  {
    FileInfo* cfinfo = (new_finfo == NULL) ? finfo : new_finfo;
    /* create a duplicate to avoid modifying the original */
    new_cinfo = (ClassInfo*)malloc(sizeof(ClassInfo));
    vtkParse_CopyClass(new_cinfo, cinfo);
    if (template_args)
    {
      vtkParse_InstantiateClassTemplate(
        new_cinfo, cfinfo->Strings, template_arg_count, template_args);
    }
    cinfo = new_cinfo;

    recurse = 0;
    if (info)
    {
      vtkParseMerge_Merge(cfinfo, info, merge, cinfo);
      recurse = 1;
    }
    else
    {
      vtkParseMerge_MergeUsing(cfinfo, info, merge, cinfo, 0);
      n = merge->NumberOfUsings;
      for (i = 0; i < n; i++)
      {
        if (merge->Usings[i]->Name)
        {
          recurse = 1;
          break;
        }
      }
    }
    if (recurse)
    {
      n = cinfo->NumberOfSuperClasses;
      for (i = 0; i < n; i++)
      {
        vtkParseMerge_MergeHelper(
          cfinfo, data, hinfo, cinfo->SuperClasses[i], nhintfiles, hintfiles, info, merge);
      }
    }
    vtkParse_FreeClass(cinfo);
    if (cfinfo != finfo)
    {
      vtkParse_FreeFile(cfinfo);
      free(cfinfo);
    }
  }

  if (template_arg_count > 0)
  {
    vtkParse_FreeTemplateDecomposition(classname, template_arg_count, template_args);
  }
}

/* Merge the methods from the superclasses */
MergeInfo* vtkParseMerge_MergeSuperClasses(
  FileInfo* finfo, const NamespaceInfo* data, ClassInfo* classInfo)
{
  HierarchyInfo* hinfo = NULL;
  MergeInfo* info = NULL;
  const OptionInfo* oinfo = vtkParse_GetCommandLineOptions();
  int i, n;

  if (oinfo->HierarchyFileNames)
  {
    hinfo =
      vtkParseHierarchy_ReadFiles(oinfo->NumberOfHierarchyFileNames, oinfo->HierarchyFileNames);

    info = vtkParseMerge_CreateMergeInfo(classInfo);

    n = classInfo->NumberOfSuperClasses;
    for (i = 0; i < n; i++)
    {
      vtkParseMerge_MergeHelper(finfo, data, hinfo, classInfo->SuperClasses[i],
        oinfo->NumberOfHintFileNames, oinfo->HintFileNames, info, classInfo);
    }
  }

  if (hinfo)
  {
    vtkParseHierarchy_Free(hinfo);
  }

  /* Do not finalize `oinfo` here; we're just peeking at global state to know
   * what hierarchy files are available. */

  return info;
}
