// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright 2008 Sandia Corporation
// SPDX-License-Identifier: LicenseRef-BSD-3-Clause-Sandia-USGov

#include "vtkSQLDatabase.h"
#include "vtkInformationObjectBaseKey.h"
#include "vtkSQLQuery.h"

#include "vtkSQLDatabaseSchema.h"

#include "vtkSQLiteDatabase.h"

#include "vtkObjectFactory.h"
#include "vtkStdString.h"

#include <mutex>
#include <sstream>
#include <vtksys/SystemTools.hxx>

VTK_ABI_NAMESPACE_BEGIN
class vtkSQLDatabase::vtkCallbackVector : public std::vector<vtkSQLDatabase::CreateFunction>
{
public:
  vtkSQLDatabase* CreateFromURL(const char* URL)
  {
    iterator iter;
    for (iter = this->begin(); iter != this->end(); ++iter)
    {
      vtkSQLDatabase* db = (*(*iter))(URL);
      if (db)
      {
        return db;
      }
    }
    return nullptr;
  }
};
vtkSQLDatabase::vtkCallbackVector* vtkSQLDatabase::Callbacks = nullptr;

// Ensures that there are no leaks when the application exits.
class vtkSQLDatabaseCleanup
{
public:
  void Use() {}
  ~vtkSQLDatabaseCleanup() { vtkSQLDatabase::UnRegisterAllCreateFromURLCallbacks(); }
};

// Used to clean up the Callbacks
static vtkSQLDatabaseCleanup vtkCleanupSQLDatabaseGlobal;

vtkInformationKeyMacro(vtkSQLDatabase, DATABASE, ObjectBase);

//------------------------------------------------------------------------------
vtkSQLDatabase::vtkSQLDatabase() = default;

//------------------------------------------------------------------------------
vtkSQLDatabase::~vtkSQLDatabase() = default;

//------------------------------------------------------------------------------
void vtkSQLDatabase::RegisterCreateFromURLCallback(vtkSQLDatabase::CreateFunction callback)
{
  if (!vtkSQLDatabase::Callbacks)
  {
    vtkCleanupSQLDatabaseGlobal.Use();
    vtkSQLDatabase::Callbacks = new vtkCallbackVector();
  }
  vtkSQLDatabase::Callbacks->push_back(callback);
}

//------------------------------------------------------------------------------
void vtkSQLDatabase::UnRegisterCreateFromURLCallback(vtkSQLDatabase::CreateFunction callback)
{
  if (vtkSQLDatabase::Callbacks)
  {
    vtkSQLDatabase::vtkCallbackVector::iterator iter;
    for (iter = vtkSQLDatabase::Callbacks->begin(); iter != vtkSQLDatabase::Callbacks->end();
         ++iter)
    {
      if ((*iter) == callback)
      {
        vtkSQLDatabase::Callbacks->erase(iter);
        break;
      }
    }
  }
}

//------------------------------------------------------------------------------
void vtkSQLDatabase::UnRegisterAllCreateFromURLCallbacks()
{
  delete vtkSQLDatabase::Callbacks;
  vtkSQLDatabase::Callbacks = nullptr;
}

//------------------------------------------------------------------------------
void vtkSQLDatabase::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//------------------------------------------------------------------------------
vtkStdString vtkSQLDatabase::GetColumnSpecification(
  vtkSQLDatabaseSchema* schema, int tblHandle, int colHandle)
{
  std::ostringstream queryStr;
  queryStr << schema->GetColumnNameFromHandle(tblHandle, colHandle);

  // Figure out column type
  int colType = schema->GetColumnTypeFromHandle(tblHandle, colHandle);
  std::string colTypeStr;
  switch (static_cast<vtkSQLDatabaseSchema::DatabaseColumnType>(colType))
  {
    case vtkSQLDatabaseSchema::SERIAL:
      colTypeStr = "INTEGER";
      break;
    case vtkSQLDatabaseSchema::SMALLINT:
      colTypeStr = "INTEGER";
      break;
    case vtkSQLDatabaseSchema::INTEGER:
      colTypeStr = "INTEGER";
      break;
    case vtkSQLDatabaseSchema::BIGINT:
      colTypeStr = "INTEGER";
      break;
    case vtkSQLDatabaseSchema::VARCHAR:
      colTypeStr = "VARCHAR";
      break;
    case vtkSQLDatabaseSchema::TEXT:
      colTypeStr = "VARCHAR";
      break;
    case vtkSQLDatabaseSchema::REAL:
      colTypeStr = "FLOAT";
      break;
    case vtkSQLDatabaseSchema::DOUBLE:
      colTypeStr = "DOUBLE";
      break;
    case vtkSQLDatabaseSchema::BLOB:
      colTypeStr = "";
      break;
    case vtkSQLDatabaseSchema::TIME:
      colTypeStr = "TIME";
      break;
    case vtkSQLDatabaseSchema::DATE:
      colTypeStr = "DATE";
      break;
    case vtkSQLDatabaseSchema::TIMESTAMP:
      colTypeStr = "TIMESTAMP";
      break;
  }

  if (!colTypeStr.empty())
  {
    queryStr << " " << colTypeStr;
  }
  else // if ( colTypeStr.size() )
  {
    vtkGenericWarningMacro("Unable to get column specification: unsupported data type " << colType);
    return {};
  }

  // Decide whether size is allowed, required, or unused
  int colSizeType = 0;
  switch (static_cast<vtkSQLDatabaseSchema::DatabaseColumnType>(colType))
  {
    case vtkSQLDatabaseSchema::SERIAL:
      colSizeType = 0;
      break;
    case vtkSQLDatabaseSchema::SMALLINT:
      colSizeType = 1;
      break;
    case vtkSQLDatabaseSchema::INTEGER:
      colSizeType = 1;
      break;
    case vtkSQLDatabaseSchema::BIGINT:
      colSizeType = 1;
      break;
    case vtkSQLDatabaseSchema::VARCHAR:
      colSizeType = -1;
      break;
    case vtkSQLDatabaseSchema::TEXT:
      colSizeType = -1;
      break;
    case vtkSQLDatabaseSchema::REAL:
      colSizeType = 0;
      break;
    case vtkSQLDatabaseSchema::DOUBLE:
      colSizeType = 0;
      break;
    case vtkSQLDatabaseSchema::BLOB:
      colSizeType = 0;
      break;
    case vtkSQLDatabaseSchema::TIME:
      colSizeType = 0;
      break;
    case vtkSQLDatabaseSchema::DATE:
      colSizeType = 0;
      break;
    case vtkSQLDatabaseSchema::TIMESTAMP:
      colSizeType = 0;
      break;
  }

  // Specify size if allowed or required
  if (colSizeType)
  {
    int colSize = schema->GetColumnSizeFromHandle(tblHandle, colHandle);
    // IF size is provided but absurd,
    // OR, if size is required but not provided OR absurd,
    // THEN assign the default size.
    if ((colSize < 0) || (colSizeType == -1 && colSize < 1))
    {
      colSize = VTK_SQL_DEFAULT_COLUMN_SIZE;
    }

    // At this point, we have either a valid size if required, or a possibly null valid size
    // if not required. Thus, skip sizing in the latter case.
    if (colSize > 0)
    {
      queryStr << "(" << colSize << ")";
    }
  }

  std::string attStr = schema->GetColumnAttributesFromHandle(tblHandle, colHandle);
  if (!attStr.empty())
  {
    queryStr << " " << attStr;
  }

  return queryStr.str();
}

//------------------------------------------------------------------------------
vtkStdString vtkSQLDatabase::GetIndexSpecification(
  vtkSQLDatabaseSchema* schema, int tblHandle, int idxHandle, bool& skipped)
{
  vtkStdString queryStr;

  int idxType = schema->GetIndexTypeFromHandle(tblHandle, idxHandle);
  switch (idxType)
  {
    case vtkSQLDatabaseSchema::PRIMARY_KEY:
      queryStr = ", PRIMARY KEY ";
      skipped = false;
      break;
    case vtkSQLDatabaseSchema::UNIQUE:
      queryStr = ", UNIQUE ";
      skipped = false;
      break;
    case vtkSQLDatabaseSchema::INDEX:
      // Not supported within a CREATE TABLE statement by all SQL backends:
      // must be created later with a CREATE INDEX statement
      queryStr = "CREATE INDEX ";
      skipped = true;
      break;
    default:
      return {};
  }

  // No index_name for PRIMARY KEYs nor UNIQUEs
  if (skipped)
  {
    queryStr += schema->GetIndexNameFromHandle(tblHandle, idxHandle);
  }

  // CREATE INDEX <index name> ON <table name> syntax
  if (skipped)
  {
    queryStr += " ON ";
    queryStr += schema->GetTableNameFromHandle(tblHandle);
  }

  queryStr += " (";

  // Loop over all column names of the index
  int numCnm = schema->GetNumberOfColumnNamesInIndex(tblHandle, idxHandle);
  if (numCnm < 0)
  {
    vtkGenericWarningMacro(
      "Unable to get index specification: index has incorrect number of columns " << numCnm);
    return {};
  }

  bool firstCnm = true;
  for (int cnmHandle = 0; cnmHandle < numCnm; ++cnmHandle)
  {
    if (firstCnm)
    {
      firstCnm = false;
    }
    else
    {
      queryStr += ",";
    }
    queryStr += schema->GetIndexColumnNameFromHandle(tblHandle, idxHandle, cnmHandle);
  }
  queryStr += ")";

  return queryStr;
}

//------------------------------------------------------------------------------
vtkStdString vtkSQLDatabase::GetTriggerSpecification(
  vtkSQLDatabaseSchema* schema, int tblHandle, int trgHandle)
{
  vtkStdString queryStr = "CREATE TRIGGER ";
  queryStr += schema->GetTriggerNameFromHandle(tblHandle, trgHandle);

  int trgType = schema->GetTriggerTypeFromHandle(tblHandle, trgHandle);
  // odd types: AFTER, even types: BEFORE
  if (trgType % 2)
  {
    queryStr += " AFTER ";
  }
  else
  {
    queryStr += " BEFORE ";
  }
  // 0/1: INSERT, 2/3: UPDATE, 4/5: DELETE
  if (trgType > 1)
  {
    if (trgType > 3)
    {
      queryStr += "DELETE ON ";
    }
    else // if ( trgType > 3 )
    {
      queryStr += "UPDATE ON ";
    }
  }
  else // if ( trgType > 1 )
  {
    queryStr += "INSERT ON ";
  }

  queryStr += schema->GetTableNameFromHandle(tblHandle);
  queryStr += " ";
  queryStr += schema->GetTriggerActionFromHandle(tblHandle, trgHandle);

  return queryStr;
}

//------------------------------------------------------------------------------
vtkSQLDatabase* vtkSQLDatabase::CreateFromURL(const char* URL)
{
  std::string urlstr(URL ? URL : "");
  std::string protocol;
  std::string username;
  std::string unused;
  std::string hostname;
  std::string dataport;
  std::string database;
  std::string dataglom;
  vtkSQLDatabase* db = nullptr;

  static std::mutex dbURLCritSec;
  dbURLCritSec.lock();

  // SQLite is a bit special so lets get that out of the way :)
  if (!vtksys::SystemTools::ParseURLProtocol(urlstr, protocol, dataglom))
  {
    vtkGenericWarningMacro("Invalid URL (no protocol found): \"" << urlstr << "\"");
    dbURLCritSec.unlock();
    return nullptr;
  }
  if (protocol == "sqlite")
  {
    db = vtkSQLiteDatabase::New();
    db->ParseURL(URL);
    dbURLCritSec.unlock();
    return db;
  }

  // Okay now for all the other database types get more detailed info
  if (!vtksys::SystemTools::ParseURL(
        urlstr, protocol, username, unused, hostname, dataport, database))
  {
    vtkGenericWarningMacro("Invalid URL (other components missing): \"" << urlstr << "\"");
    dbURLCritSec.unlock();
    return nullptr;
  }

  // Now try to look at registered callback to try and find someone who can
  // provide us with the required implementation.
  if (!db && vtkSQLDatabase::Callbacks)
  {
    db = vtkSQLDatabase::Callbacks->CreateFromURL(URL);
  }

  if (!db)
  {
    vtkGenericWarningMacro("Unsupported protocol: " << protocol);
  }
  dbURLCritSec.unlock();
  return db;
}

//------------------------------------------------------------------------------
bool vtkSQLDatabase::EffectSchema(vtkSQLDatabaseSchema* schema, bool dropIfExists)
{
  if (!this->IsOpen())
  {
    vtkGenericWarningMacro("Unable to effect the schema: no database is open");
    return false;
  }

  // Instantiate an empty query and begin the transaction.
  vtkSQLQuery* query = this->GetQueryInstance();
  if (!query->BeginTransaction())
  {
    vtkGenericWarningMacro("Unable to effect the schema: unable to begin transaction");
    return false;
  }

  // Loop over preamble statements of the schema and execute them only if they are relevant
  int numPre = schema->GetNumberOfPreambles();
  for (int preHandle = 0; preHandle < numPre; ++preHandle)
  {
    // Don't execute if the statement is not for this backend
    const char* preBackend = schema->GetPreambleBackendFromHandle(preHandle);
    if (strcmp(preBackend, VTK_SQL_ALLBACKENDS) != 0 &&
      strcmp(preBackend, this->GetClassName()) != 0)
    {
      continue;
    }

    vtkStdString preStr = schema->GetPreambleActionFromHandle(preHandle);
    query->SetQuery(preStr.c_str());
    if (!query->Execute())
    {
      vtkGenericWarningMacro("Unable to effect the schema: unable to execute query.\nDetails: "
        << query->GetLastErrorText());
      query->RollbackTransaction();
      query->Delete();
      return false;
    }
  }

  // Loop over all tables of the schema and create them
  int numTbl = schema->GetNumberOfTables();
  for (int tblHandle = 0; tblHandle < numTbl; ++tblHandle)
  {
    // Construct the CREATE TABLE query for this table
    std::string queryStr("CREATE TABLE ");
    queryStr += this->GetTablePreamble(dropIfExists);
    queryStr += schema->GetTableNameFromHandle(tblHandle);
    queryStr += " (";

    // Loop over all columns of the current table
    int numCol = schema->GetNumberOfColumnsInTable(tblHandle);
    if (numCol < 0)
    {
      query->RollbackTransaction();
      query->Delete();
      return false;
    }

    bool firstCol = true;
    for (int colHandle = 0; colHandle < numCol; ++colHandle)
    {
      if (!firstCol)
      {
        queryStr += ", ";
      }
      else // ( ! firstCol )
      {
        firstCol = false;
      }

      // Get column creation syntax (backend-dependent)
      std::string colStr = this->GetColumnSpecification(schema, tblHandle, colHandle);
      if (!colStr.empty())
      {
        queryStr += colStr;
      }
      else // if ( colStr.size() )
      {
        query->RollbackTransaction();
        query->Delete();
        return false;
      }
    }

    // Check out number of indices
    int numIdx = schema->GetNumberOfIndicesInTable(tblHandle);
    if (numIdx < 0)
    {
      query->RollbackTransaction();
      query->Delete();
      return false;
    }

    // In case separate INDEX statements are needed (backend-specific)
    std::vector<std::string> idxStatements;
    bool skipped = false;

    // Loop over all indices of the current table
    for (int idxHandle = 0; idxHandle < numIdx; ++idxHandle)
    {
      // Get index creation syntax (backend-dependent)
      std::string idxStr = this->GetIndexSpecification(schema, tblHandle, idxHandle, skipped);
      if (!idxStr.empty())
      {
        if (skipped)
        {
          // Must create this index later
          idxStatements.push_back(idxStr);
          continue;
        }
        else // if ( skipped )
        {
          queryStr += idxStr;
        }
      }
      else // if ( idxStr.size() )
      {
        query->RollbackTransaction();
        query->Delete();
        return false;
      }
    }
    queryStr += ")";

    // Add options to the end of the CREATE TABLE statement
    int numOpt = schema->GetNumberOfOptionsInTable(tblHandle);
    if (numOpt < 0)
    {
      query->RollbackTransaction();
      query->Delete();
      return false;
    }
    for (int optHandle = 0; optHandle < numOpt; ++optHandle)
    {
      vtkStdString optBackend = schema->GetOptionBackendFromHandle(tblHandle, optHandle);
      if (optBackend != VTK_SQL_ALLBACKENDS && optBackend != this->GetClassName())
      {
        continue;
      }
      queryStr += " ";
      queryStr += schema->GetOptionTextFromHandle(tblHandle, optHandle);
    }

    // Execute the CREATE TABLE query
    query->SetQuery(queryStr.c_str());
    if (!query->Execute())
    {
      vtkGenericWarningMacro("Unable to effect the schema: unable to execute query.\nDetails: "
        << query->GetLastErrorText());
      query->RollbackTransaction();
      query->Delete();
      return false;
    }

    // Execute separate CREATE INDEX statements if needed
    for (std::vector<std::string>::iterator it = idxStatements.begin(); it != idxStatements.end();
         ++it)
    {
      query->SetQuery(it->c_str());
      if (!query->Execute())
      {
        vtkGenericWarningMacro("Unable to effect the schema: unable to execute query.\nDetails: "
          << query->GetLastErrorText());
        query->RollbackTransaction();
        query->Delete();
        return false;
      }
    }

    // Check out number of triggers
    int numTrg = schema->GetNumberOfTriggersInTable(tblHandle);
    if (numTrg < 0)
    {
      query->RollbackTransaction();
      query->Delete();
      return false;
    }

    // Construct CREATE TRIGGER statements only if they are supported by the backend at hand
    if (numTrg && IsSupported(VTK_SQL_FEATURE_TRIGGERS))
    {
      // Loop over all triggers of the current table
      for (int trgHandle = 0; trgHandle < numTrg; ++trgHandle)
      {
        // Don't execute if the trigger is not for this backend
        const char* trgBackend = schema->GetTriggerBackendFromHandle(tblHandle, trgHandle);
        if (strcmp(trgBackend, VTK_SQL_ALLBACKENDS) != 0 &&
          strcmp(trgBackend, this->GetClassName()) != 0)
        {
          continue;
        }

        // Get trigger creation syntax (backend-dependent)
        std::string trgStr = this->GetTriggerSpecification(schema, tblHandle, trgHandle);

        // If not empty, execute query
        if (!trgStr.empty())
        {
          query->SetQuery(trgStr.c_str());
          if (!query->Execute())
          {
            vtkGenericWarningMacro(
              "Unable to effect the schema: unable to execute query.\nDetails: "
              << query->GetLastErrorText());
            query->RollbackTransaction();
            query->Delete();
            return false;
          }
        }
        else // if ( trgStr.size() )
        {
          query->RollbackTransaction();
          query->Delete();
          return false;
        }
      }
    }

    // If triggers are specified but not supported, don't quit, but let the user know it
    else if (numTrg)
    {
      vtkGenericWarningMacro("Triggers are not supported by this SQL backend; ignoring them.");
    }
  } //  for ( int tblHandle = 0; tblHandle < numTbl; ++ tblHandle )

  // Commit the transaction.
  if (!query->CommitTransaction())
  {
    vtkGenericWarningMacro("Unable to effect the schema: unable to commit transaction.\nDetails: "
      << query->GetLastErrorText());
    query->Delete();
    return false;
  }

  query->Delete();
  return true;
}
VTK_ABI_NAMESPACE_END
