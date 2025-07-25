// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
/*
  When wrapping overloaded C++ methods, it is necessary to provide
  hints so that Python can choose which overload to call (see
  vtkPythonOverload.cxx for the code that is used to do this).

  Where possible, overloads are resolved based on the number of
  arguments that are passed.  When this isn't possible, the overloads
  must be resolved based on argument types.  So, for each overload,
  we store the parameter types as a string.

  The "parameter type" string can start with one of the following:

    - (hyphen) marks a method as an explicit constructor
    @ placeholder for "self" in a method (i.e. method is not static)

  For each parameter, one of the following codes is used:

    q bool
    c char
    b signed char
    B unsigned char
    h signed short
    H unsigned short
    i int
    I unsigned int
    l long
    L unsigned long
    k long long
    K unsigned long long
    f float
    d double
    v void *
    z char *
    s string
    u unicode
    F callable object
    E enum type
    O python object
    Q Qt object
    V VTK object
    W VTK special type
    P Pointer to numeric type
    A Multi-dimensional array of numeric type
    T std::vector

    | marks the end of required parameters, following parameters are optional

  If the parameter is E, O, Q, V, W, then a type name must follow the type
  codes. The type name must be preceded by '*' if the type is a non-const
  reference or a pointer.  For example,

    func(vtkArray *, vtkVariant &, int) -> "VWi *vtkArray &vtkVariant"

  If the parameter is P, then the type of the array or pointer must
  follow the type codes.  For example,

    func(int *p, double a[10]) -> "PP *i *d"

  If the parameter is A, then both the type and all dimensions after the
  first dimension must be provided:

    func(double a[3][4]) -> "A *d[4]"

*/

#include "vtkWrapPythonOverload.h"
#include "vtkWrapPythonMethod.h"
#include "vtkWrapPythonTemplate.h"

#include "vtkParseExtras.h"
#include "vtkWrap.h"
#include "vtkWrapText.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------- */
/* prototypes for utility methods */

/* create a parameter format char for the give type */
static char vtkWrapPython_FormatChar(unsigned int argtype);

/* create a string for checking arguments against available signatures */
static char* vtkWrapPython_ArgCheckString(const ClassInfo* data, FunctionInfo* currentFunction);

/* -------------------------------------------------------------------- */
/* Get the python format char for the give type, after retrieving the
 * base type from the type */
static char vtkWrapPython_FormatChar(unsigned int argtype)
{
  char typeChar = 'O';

  switch ((argtype & VTK_PARSE_BASE_TYPE))
  {
    case VTK_PARSE_FLOAT:
      typeChar = 'f';
      break;
    case VTK_PARSE_DOUBLE:
      typeChar = 'd';
      break;
    case VTK_PARSE_UNSIGNED_INT:
      typeChar = 'I';
      break;
    case VTK_PARSE_INT:
      typeChar = 'i';
      break;
    case VTK_PARSE_UNSIGNED_SHORT:
      typeChar = 'H';
      break;
    case VTK_PARSE_SHORT:
      typeChar = 'h';
      break;
    case VTK_PARSE_UNSIGNED_LONG:
      typeChar = 'L';
      break;
    case VTK_PARSE_LONG:
      typeChar = 'l';
      break;
    case VTK_PARSE_SIZE_T:
    case VTK_PARSE_UNSIGNED_LONG_LONG:
      typeChar = 'K';
      break;
    case VTK_PARSE_SSIZE_T:
    case VTK_PARSE_LONG_LONG:
      typeChar = 'k';
      break;
    case VTK_PARSE_SIGNED_CHAR:
      typeChar = 'b';
      break;
    case VTK_PARSE_CHAR:
      typeChar = 'c';
      break;
    case VTK_PARSE_UNSIGNED_CHAR:
      typeChar = 'B';
      break;
    case VTK_PARSE_VOID:
      typeChar = 'v';
      break;
    case VTK_PARSE_BOOL:
      typeChar = 'q';
      break;
    case VTK_PARSE_STRING:
      typeChar = 's';
      break;
  }

  return typeChar;
}

/* -------------------------------------------------------------------- */
/* Create a string to describe the signature of a method. */

static char* vtkWrapPython_ArgCheckString(const ClassInfo* data, FunctionInfo* currentFunction)
{
  static char result[2048]; /* max literal string length */
  char classname[1024];
  size_t currPos = 0;
  size_t endPos;
  ValueInfo* arg;
  unsigned int argtype;
  int i, j, k;
  int totalArgs, requiredArgs;
  char c = '\0';

  totalArgs = vtkWrap_CountWrappedParameters(currentFunction);
  requiredArgs = vtkWrap_CountRequiredArguments(currentFunction);

  if (currentFunction->IsExplicit)
  {
    /* used to mark constructors as 'explicit' */
    result[currPos++] = '-';
  }

  /* placeholder for 'self' in method calls */
  if (!currentFunction->IsStatic)
  {
    result[currPos++] = '@';
  }

  /* position for insertion after format chars */
  endPos = currPos + totalArgs;
  if (totalArgs > requiredArgs)
  {
    /* add one for the "|" that marks the end of required args */
    endPos++;
  }

  /* create a format character for each argument */
  for (i = 0; i < totalArgs; i++)
  {
    arg = currentFunction->Parameters[i];
    argtype = (arg->Type & VTK_PARSE_UNQUALIFIED_TYPE);

    if (i == requiredArgs)
    {
      /* make all following arguments optional */
      result[currPos++] = '|';
    }

    /* will store the classname for objects */
    classname[0] = '\0';

    if (vtkWrap_IsEnumMember(data, arg))
    {
      c = 'E';
      snprintf(classname, sizeof(classname), "%.200s.%.200s", data->Name, arg->Class);
    }
    else if (arg->IsEnum)
    {
      c = 'E';
      vtkWrapText_PythonName(arg->Class, classname);
    }
    else if (vtkWrap_IsPythonObject(arg))
    {
      c = 'O';
      vtkWrapText_PythonName(arg->Class, classname);
    }
    else if (vtkWrap_IsVTKObject(arg))
    {
      c = 'V';
      vtkWrapText_PythonName(arg->Class, classname);
    }
    else if (vtkWrap_IsVTKSmartPointer(arg))
    {
      char* templateArg = vtkWrap_TemplateArg(arg->Class);
      argtype = VTK_PARSE_OBJECT_PTR;
      c = 'V';
      vtkWrapText_PythonName(templateArg, classname);
      free(templateArg);
    }
    else if (vtkWrap_IsSpecialObject(arg))
    {
      c = 'W';
      vtkWrapText_PythonName(arg->Class, classname);
    }
    else if (vtkWrap_IsFunction(arg))
    {
      c = 'F';
    }
    else if (vtkWrap_IsVoidPointer(arg))
    {
      c = 'v';
    }
    else if (vtkWrap_IsString(arg))
    {
      c = 's';
    }
    else if (vtkWrap_IsCharPointer(arg))
    {
      c = 'z';
    }
    else if (vtkWrap_IsNumeric(arg) && vtkWrap_IsScalar(arg))
    {
      c = vtkWrapPython_FormatChar(argtype);
    }
    else if (vtkWrap_IsArray(arg) || vtkWrap_IsPODPointer(arg))
    {
      c = 'P';
      result[endPos++] = ' ';
      result[endPos++] = '*';
      result[endPos++] = vtkWrapPython_FormatChar(argtype);
    }
    else if (vtkWrap_IsNArray(arg))
    {
      c = 'A';
      result[endPos++] = ' ';
      result[endPos++] = '*';
      result[endPos++] = vtkWrapPython_FormatChar(argtype);
      if (vtkWrap_IsNArray(arg))
      {
        for (j = 1; j < arg->NumberOfDimensions; j++)
        {
          result[endPos++] = '[';
          for (k = 0; arg->Dimensions[j][k]; k++)
          {
            result[endPos++] = arg->Dimensions[j][k];
          }
          result[endPos++] = ']';
        }
      }
    }
    else if (vtkWrap_IsStdVector(arg))
    {
      /* tclass, ttype will hold the value type of the vector */
      const char* tclass;
      unsigned int ttype;
      /* first, decompose template into template name + args */
      const char* tname;                      /* for template name, "std::vector" */
      size_t n;                               /* for length of tname string */
      const char** targs;                     /* for template args */
      const char* defaults[2] = { NULL, "" }; /* NULL means "not optional" */
      const size_t m = 16;                    /* length of "vtkSmartPointer<" */
      vtkParse_DecomposeTemplatedType(arg->Class, &tname, 2, &targs, defaults);
      vtkParse_BasicTypeFromString(targs[0], &ttype, &tclass, &n);
      c = 'T';
      result[endPos++] = ' ';
      if (ttype == VTK_PARSE_OBJECT && strncmp(tclass, "vtkSmartPointer<", m) == 0)
      {
        /* The '*' indicates a pointer (in this case, a vtkSmartPointer) */
        result[endPos++] = '*';
        /* get the VTK object type "T" from "vtkSmartPointer<T>" */
        vtkParse_BasicTypeFromString(&tclass[m], &ttype, &tclass, &n);
        memcpy(&result[endPos], tclass, n);
        endPos += n;
      }
      else
      {
        /* for vectors of anything that isn't a vtkSmartPointer */
        result[endPos++] = vtkWrapPython_FormatChar(ttype);
      }
      vtkParse_FreeTemplateDecomposition(tname, 2, targs);
    }

    /* add the format char to the string */
    result[currPos++] = c;
    if (classname[0] != '\0')
    {
      result[endPos++] = ' ';
      if ((argtype == VTK_PARSE_OBJECT_REF || argtype == VTK_PARSE_QOBJECT_REF ||
            argtype == VTK_PARSE_UNKNOWN_REF) &&
        (arg->Type & VTK_PARSE_CONST) == 0)
      {
        result[endPos++] = '&';
      }
      else if (argtype == VTK_PARSE_OBJECT_PTR || argtype == VTK_PARSE_UNKNOWN_PTR ||
        argtype == VTK_PARSE_QOBJECT_PTR)
      {
        result[endPos++] = '*';
      }
      strcpy(&result[endPos], classname);
      endPos += strlen(classname);
    }
  }

  result[endPos] = '\0';
  return result;
}

/* -------------------------------------------------------------------- */
/* Generate an int array that maps arg counts to overloads.
 * Each element in the array will either contain the index of the
 * overload that it maps to, or -1 if it maps to multiple overloads,
 * or zero if it does not map to any.  The length of the array is
 * returned in "nmax". The value of "overlap" is set to 1 if there
 * are some arg counts that map to more than one method. */

int* vtkWrapPython_ArgCountToOverloadMap(WrappedFunction* wrappedFunctions,
  int numberOfWrappedFunctions, int fnum, int is_vtkobject, int* nmax, int* overlap)
{
  static int overloadMap[512];
  int totalArgs, requiredArgs;
  int occ, occCounter;
  const FunctionInfo* theOccurrence;
  const FunctionInfo* theFunc;
  int mixed_static, any_static;
  int i;

  *nmax = 0;
  *overlap = 0;

  theFunc = wrappedFunctions[fnum].Archetype;

  any_static = 0;
  mixed_static = 0;
  for (i = fnum; i < numberOfWrappedFunctions; i++)
  {
    theOccurrence = wrappedFunctions[i].Archetype;
    if (theOccurrence && strcmp(theOccurrence->Name, theFunc->Name) == 0)
    {
      if (theOccurrence->IsStatic)
      {
        any_static = 1;
      }
      else if (any_static)
      {
        mixed_static = 1;
      }
    }
  }

  for (i = 0; i < 100; i++)
  {
    overloadMap[i] = 0;
  }

  occCounter = 0;
  for (occ = fnum; occ < numberOfWrappedFunctions; occ++)
  {
    theOccurrence = wrappedFunctions[occ].Archetype;

    if (theOccurrence == NULL || strcmp(theOccurrence->Name, theFunc->Name) != 0)
    {
      continue;
    }

    occCounter++;

    totalArgs = vtkWrap_CountWrappedParameters(theOccurrence);
    requiredArgs = vtkWrap_CountRequiredArguments(theOccurrence);

    /* vtkobject calls might have an extra "self" arg in front */
    if (mixed_static && is_vtkobject && !theOccurrence->IsStatic)
    {
      totalArgs++;
    }

    if (totalArgs > *nmax)
    {
      *nmax = totalArgs;
    }

    for (i = requiredArgs; i <= totalArgs && i < 100; i++)
    {
      if (overloadMap[i] == 0)
      {
        overloadMap[i] = occCounter;
      }
      else
      {
        overloadMap[i] = -1;
        *overlap = 1;
      }
    }
  }

  return overloadMap;
}

/* -------------------------------------------------------------------- */
/* output the method table for all overloads of a particular method,
 * this is also used to write out all constructors for the class */

void vtkWrapPython_OverloadMethodDef(FILE* fp, const char* classname, const ClassInfo* data,
  const int* overloadMap, WrappedFunction* wrappedFunctions, int numberOfWrappedFunctions, int fnum,
  int numberOfOccurrences)
{
  char occSuffix[16];
  int occ, occCounter;
  FunctionInfo* theOccurrence;
  const FunctionInfo* theFunc;
  int totalArgs, requiredArgs;
  int i;
  int putInTable;

  theFunc = wrappedFunctions[fnum].Archetype;

  fprintf(fp, "static PyMethodDef Py%s_%s_Methods[] = {\n", classname, theFunc->Name);

  occCounter = 0;
  for (occ = fnum; occ < numberOfWrappedFunctions; occ++)
  {
    theOccurrence = wrappedFunctions[occ].Archetype;

    if (theOccurrence == NULL || strcmp(theOccurrence->Name, theFunc->Name) != 0)
    {
      continue;
    }

    occCounter++;

    totalArgs = vtkWrap_CountWrappedParameters(theOccurrence);
    requiredArgs = vtkWrap_CountRequiredArguments(theOccurrence);

    putInTable = 0;

    /* all conversion constructors must go into the table */
    if (vtkWrap_IsConstructor(data, theOccurrence) && requiredArgs <= 1 && totalArgs >= 1 &&
      !theOccurrence->IsExplicit)
    {
      putInTable = 1;
    }

    /* all methods that overlap with others must go in the table */
    for (i = requiredArgs; i <= totalArgs; i++)
    {
      if (overloadMap[i] == -1)
      {
        putInTable = 1;
      }
    }

    if (!putInTable)
    {
      continue;
    }

    /* method suffix to distinguish between signatures */
    occSuffix[0] = '\0';
    if (numberOfOccurrences > 1)
    {
      snprintf(occSuffix, sizeof(occSuffix), "_s%d", occCounter);
    }

    fprintf(fp,
      "  {\"%s\", Py%s_%s%s, METH_VARARGS%s,\n"
      "   \"%s\"},\n",
      theOccurrence->Name, classname, theOccurrence->Name, occSuffix,
      theOccurrence->IsStatic ? " | METH_STATIC" : "",
      vtkWrapPython_ArgCheckString(data, theOccurrence));
  }

  fprintf(fp,
    "  {nullptr, nullptr, 0, nullptr}\n"
    "};\n"
    "\n");
}

/* -------------------------------------------------------------------- */
/* make a method that will choose which overload to call */

void vtkWrapPython_OverloadMasterMethod(FILE* fp, const char* classname, const int* overloadMap,
  int maxArgs, WrappedFunction* wrappedFunctions, int numberOfWrappedFunctions, int fnum,
  int is_vtkobject)
{
  const FunctionInfo* currentFunction;
  const FunctionInfo* theOccurrence;
  int overlap = 0;
  int occ, occCounter;
  int i;
  int foundOne;
  int any_static = 0;

  currentFunction = wrappedFunctions[fnum].Archetype;

  for (occ = fnum; occ < numberOfWrappedFunctions; occ++)
  {
    theOccurrence = wrappedFunctions[occ].Archetype;
    if (theOccurrence && strcmp(theOccurrence->Name, currentFunction->Name) == 0)
    {
      if (theOccurrence->IsStatic)
      {
        any_static = 1;
      }
    }
  }

  for (i = 0; i <= maxArgs; i++)
  {
    if (overloadMap[i] == -1)
    {
      overlap = 1;
    }
  }

  fprintf(fp,
    "static PyObject *\n"
    "Py%s_%s(PyObject *self, PyObject *args)\n"
    "{\n",
    classname, currentFunction->Name);

  if (overlap)
  {
    fprintf(fp, "  PyMethodDef *methods = Py%s_%s_Methods;\n", classname, currentFunction->Name);
  }

  fprintf(fp,
    "  int nargs = vtkPythonArgs::GetArgCount(%sargs);\n"
    "\n",
    ((is_vtkobject && !any_static) ? "self, " : ""));

  fprintf(fp,
    "  switch(nargs)\n"
    "  {\n");

  /* find all occurrences of this method */
  occCounter = 0;
  for (occ = fnum; occ < numberOfWrappedFunctions; occ++)
  {
    theOccurrence = wrappedFunctions[occ].Archetype;

    /* is it the same name */
    if (theOccurrence && strcmp(currentFunction->Name, theOccurrence->Name) == 0)
    {
      occCounter++;

      foundOne = 0;
      for (i = 0; i <= maxArgs; i++)
      {
        if (overloadMap[i] == occCounter)
        {
          fprintf(fp, "    case %d:\n", i);
          foundOne = 1;
        }
      }
      if (foundOne)
      {
        fprintf(fp, "      return Py%s_%s_s%d(self, args);\n", classname, currentFunction->Name,
          occCounter);
      }
    }
  }

  if (overlap)
  {
    for (i = 0; i <= maxArgs; i++)
    {
      if (overloadMap[i] == -1)
      {
        fprintf(fp, "    case %d:\n", i);
      }
    }
    fprintf(fp, "      return vtkPythonOverload::CallMethod(methods, self, args);\n");
  }

  fprintf(fp,
    "  }\n"
    "\n");

  fprintf(fp, "  vtkPythonArgs::ArgCountError(nargs, \"%.200s\");\n", currentFunction->Name);

  fprintf(fp,
    "  return nullptr;\n"
    "}\n"
    "\n");
}
