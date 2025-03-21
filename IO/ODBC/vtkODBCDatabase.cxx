// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright (c) Sandia Corporation
// SPDX-License-Identifier: BSD-3-Clause
/*
 * Microsoft's own version of sqltypes.h relies on some typedefs and
 * macros in windows.h.  This next fragment tells VTK to include the
 * whole thing without any of its usual #defines to keep the size
 * manageable.  No WIN32_LEAN_AND_MEAN for us!
 */
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <vtkWindows.h>
#endif

#include "vtkSQLDatabaseSchema.h"

#include "vtkODBCDatabase.h"
#include "vtkODBCInternals.h"
#include "vtkODBCQuery.h"

#include "vtkObjectFactory.h"
#include "vtkStringArray.h"

#include <sstream>
#include <vtksys/SystemTools.hxx>

#include <cassert>
#include <cstring>
#include <vector>

#include <sql.h>
#include <sqlext.h>

//------------------------------------------------------------------------------
VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkODBCDatabase);

//------------------------------------------------------------------------------
static std::string GetErrorMessage(SQLSMALLINT handleType, SQLHANDLE handle, int* code = nullptr)
{
  SQLINTEGER sqlNativeCode = 0;
  SQLSMALLINT messageLength = 0;
  SQLRETURN status;
  SQLCHAR state[SQL_SQLSTATE_SIZE + 1];
  SQLCHAR description[SQL_MAX_MESSAGE_LENGTH + 1];
  int i = 1;

  // There may be several error messages queued up so we need to loop
  // until we've got everything.
  std::ostringstream messagebuf;
  do
  {
    status = SQLGetDiagRec(handleType, handle, i, state, &sqlNativeCode, description,
      SQL_MAX_MESSAGE_LENGTH, &messageLength);

    description[SQL_MAX_MESSAGE_LENGTH] = 0;
    if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO)
    {
      if (code)
      {
        *code = sqlNativeCode;
      }
      if (i > 1)
      {
        messagebuf << ", ";
      }
      messagebuf << state << ' ' << description;
    }
    else if (status == SQL_ERROR || status == SQL_INVALID_HANDLE)
    {
      return messagebuf.str();
    }
    ++i;
  } while (status != SQL_NO_DATA);

  return messagebuf.str();
}

//------------------------------------------------------------------------------
// COLUMN is zero-indexed but ODBC indexes from 1.  Sigh.  Aren't
// standards fun?
//
// Also, this will need to be updated when we start handling Unicode
// characters.

static std::string odbcGetString(SQLHANDLE statement, int column, int columnSize)
{
  std::string returnString;
  SQLRETURN status = SQL_ERROR;
  SQLLEN lengthIndicator;

  // Make sure we've got room to store the results but don't go past 64k
  if (columnSize <= 0)
  {
    columnSize = 1024;
  }
  else if (columnSize > 65536)
  {
    columnSize = 65536;
  }
  else
  {
    // make room for the null terminator
    ++columnSize;
  }

  std::vector<char> buffer(columnSize);
  while (true)
  {
    status = SQLGetData(statement, column + 1, SQL_C_CHAR, static_cast<SQLPOINTER>(buffer.data()),
      columnSize, &lengthIndicator);
    if (status == SQL_SUCCESS || status == SQL_SUCCESS_WITH_INFO)
    {
      if (lengthIndicator == SQL_NULL_DATA || lengthIndicator == SQL_NO_TOTAL)
      {
        break;
      }
      int resultSize = 0;
      if (status == SQL_SUCCESS_WITH_INFO)
      {
        // SQL_SUCCESS_WITH_INFO means that there's more data to
        // retrieve so we have to do it in chunks -- hence the while
        // loop.
        resultSize = columnSize - 1;
      }
      else
      {
        resultSize = lengthIndicator;
      }
      buffer[resultSize] = 0;
      returnString += buffer.data();
    }
    else if (status == SQL_NO_DATA)
    {
      // we're done
      break;
    }
    else
    {
      cerr << "odbcGetString: Error " << status << " in SQLGetData\n";

      break;
    }
  }

  return returnString;
}

//------------------------------------------------------------------------------
vtkODBCDatabase::vtkODBCDatabase()
{
  this->Internals = new vtkODBCInternals;

  this->Tables = vtkStringArray::New();
  this->Tables->Register(this);
  this->Tables->Delete();
  this->LastErrorText = nullptr;

  this->Record = vtkStringArray::New();
  this->Record->Register(this);
  this->Record->Delete();

  this->UserName = nullptr;
  this->HostName = nullptr;
  this->DataSourceName = nullptr;
  this->DatabaseName = nullptr;
  this->Password = nullptr;

  this->ServerPort = -1; // use whatever the driver defaults to

  // Initialize instance variables
  this->DatabaseType = nullptr;
  this->SetDatabaseType("ODBC");
}

//------------------------------------------------------------------------------
vtkODBCDatabase::~vtkODBCDatabase()
{
  if (this->IsOpen())
  {
    this->Close();
  }
  if (this->DatabaseType)
  {
    this->SetDatabaseType(nullptr);
  }
  this->SetLastErrorText(nullptr);
  this->SetUserName(nullptr);
  this->SetHostName(nullptr);
  this->SetPassword(nullptr);
  this->SetDataSourceName(nullptr);
  this->SetDatabaseName(nullptr);
  delete this->Internals;

  this->Tables->UnRegister(this);
  this->Record->UnRegister(this);
}

//------------------------------------------------------------------------------
bool vtkODBCDatabase::IsSupported(int feature)
{
  switch (feature)
  {
    case VTK_SQL_FEATURE_BATCH_OPERATIONS:
    case VTK_SQL_FEATURE_NAMED_PLACEHOLDERS:
      return false;

    case VTK_SQL_FEATURE_POSITIONAL_PLACEHOLDERS:
      return true;

    case VTK_SQL_FEATURE_PREPARED_QUERIES:
    {
      return true;
    }

    case VTK_SQL_FEATURE_UNICODE:
      return false; // not until we have vtkStdWideString

    case VTK_SQL_FEATURE_QUERY_SIZE:
    case VTK_SQL_FEATURE_BLOB:
    case VTK_SQL_FEATURE_LAST_INSERT_ID:
    case VTK_SQL_FEATURE_TRANSACTIONS:
      return true;

    default:
    {
      vtkErrorMacro(<< "Unknown SQL feature code " << feature << "!  See "
                    << "vtkSQLDatabase.h for a list of possible features.");
      return false;
    };
  }
}

//------------------------------------------------------------------------------
bool vtkODBCDatabase::Open(const char* password)
{
  if (!this->DataSourceName)
  {
    this->SetLastErrorText("Cannot open database because database ID is null.");
    vtkErrorMacro(<< this->GetLastErrorText());
    return false;
  }

  if (this->IsOpen())
  {
    vtkGenericWarningMacro("Open(): Database is already open.");
    return true;
  }

  SQLRETURN status;
  status = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &(this->Internals->Environment));

  if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO)
  {
    // We don't actually have a valid SQL handle yet so I don't think
    // we can actually retrieve an error message.
    std::ostringstream sbuf;
    sbuf << "vtkODBCDatabase::Open: Unable to allocate environment handle.  "
         << "Return code " << status
         << ", error message: " << GetErrorMessage(SQL_HANDLE_ENV, this->Internals->Environment);
    this->SetLastErrorText(sbuf.str().c_str());
    return false;
  }
  else
  {
    vtkDebugMacro(<< "Successfully allocated environment handle.");
    status = SQLSetEnvAttr(this->Internals->Environment, SQL_ATTR_ODBC_VERSION,
      reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), SQL_IS_UINTEGER);
  }

  // Create the connection string itself
  std::string connectionString;
  if (strstr(this->DataSourceName, ".dsn") != nullptr)
  {
    // the data source is a file of some sort
    connectionString = "FILEDSN=";
    connectionString += this->DataSourceName;
  }
  else if (strstr(this->DataSourceName, "DRIVER") != nullptr ||
    strstr(this->DataSourceName, "SERVER"))
  {
    connectionString = this->DataSourceName;
  }
  else
  {
    connectionString = "DSN=";
    connectionString += this->DataSourceName;
  }

  if (this->UserName != nullptr && strlen(this->UserName) > 0)
  {
    connectionString += ";UID=";
    connectionString += this->UserName;
  }
  if (password != nullptr)
  {
    connectionString += ";PWD=";
    connectionString += password;
  }
  if (this->DatabaseName != nullptr && strlen(this->DatabaseName) > 0)
  {
    connectionString += ";DATABASE=";
    connectionString += this->DatabaseName;
  }

  // Get a handle to connect with
  status =
    SQLAllocHandle(SQL_HANDLE_DBC, this->Internals->Environment, &(this->Internals->Connection));

  if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO)
  {
    std::ostringstream errbuf;
    errbuf << "Error allocating ODBC connection handle: "
           << GetErrorMessage(SQL_HANDLE_ENV, this->Internals->Environment);
    this->SetLastErrorText(errbuf.str().c_str());
    return false;
  }

  vtkDebugMacro(<< "ODBC connection handle successfully allocated");

#ifdef ODBC_DRIVER_IS_IODBC
  // Set the driver name so we know who to blame
  std::string driverName("vtkODBCDatabase driver");
  status = SQLSetConnectAttr(
    this->Internals->Connection, SQL_APPLICATION_NAME, driverName.c_str(), driverName.size());
  if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO)
  {
    std::ostringstream errbuf;
    errbuf << "Error setting driver name: "
           << GetErrorMessage(SQL_HANDLE_DBC, this->Internals->Connection);
    this->SetLastErrorText(errbuf.str().c_str());
    return false;
  }
  else
  {
    vtkDebugMacro(<< "Successfully set driver name on connect string.");
  }
#endif

  SQLSMALLINT cb;
  SQLTCHAR connectionOut[1024];
  status = SQLDriverConnect(this->Internals->Connection, nullptr,
    (SQLCHAR*)(const_cast<char*>(connectionString.c_str())),
    static_cast<SQLSMALLINT>(connectionString.size()), connectionOut, 1024, &cb,
    SQL_DRIVER_NOPROMPT);

  if (status != SQL_SUCCESS && status != SQL_SUCCESS_WITH_INFO)
  {
    std::ostringstream sbuf;
    sbuf << "vtkODBCDatabase::Open: Error during connection: "
         << GetErrorMessage(SQL_HANDLE_DBC, this->Internals->Connection);
    this->SetLastErrorText(sbuf.str().c_str());
    return false;
  }

  vtkDebugMacro(<< "Connection successful.");

  return true;
}

//------------------------------------------------------------------------------
void vtkODBCDatabase::Close()
{
  if (!this->IsOpen())
  {
    return; // not an error
  }
  else
  {
    SQLRETURN status;

    if (this->Internals->Connection != SQL_NULL_HDBC)
    {
      status = SQLDisconnect(this->Internals->Connection);
      if (status != SQL_SUCCESS)
      {
        vtkWarningMacro(<< "ODBC Close: Unable to disconnect data source");
      }
      status = SQLFreeHandle(SQL_HANDLE_DBC, this->Internals->Connection);
      if (status != SQL_SUCCESS)
      {
        vtkWarningMacro(<< "ODBC Close: Unable to free connection handle");
      }
      this->Internals->Connection = nullptr;
    }

    if (this->Internals->Environment != SQL_NULL_HENV)
    {
      status = SQLFreeHandle(SQL_HANDLE_ENV, this->Internals->Environment);
      if (status != SQL_SUCCESS)
      {
        vtkWarningMacro(<< "ODBC Close: Unable to free environment handle");
      }
      this->Internals->Environment = nullptr;
    }
  }
}

//------------------------------------------------------------------------------
bool vtkODBCDatabase::IsOpen()
{
  return (this->Internals->Connection != SQL_NULL_HDBC);
}

//------------------------------------------------------------------------------
vtkSQLQuery* vtkODBCDatabase::GetQueryInstance()
{
  vtkODBCQuery* query = vtkODBCQuery::New();
  query->SetDatabase(this);
  return query;
}

//------------------------------------------------------------------------------
const char* vtkODBCDatabase::GetLastErrorText()
{
  return this->LastErrorText;
}

//------------------------------------------------------------------------------
vtkStringArray* vtkODBCDatabase::GetTables()
{
  this->Tables->Resize(0);
  if (!this->IsOpen())
  {
    vtkErrorMacro(<< "GetTables(): Database is closed!");
    return this->Tables;
  }
  else
  {
    SQLHANDLE statement;
    SQLRETURN status = SQLAllocHandle(SQL_HANDLE_STMT, this->Internals->Connection, &statement);

    if (status != SQL_SUCCESS)
    {
      vtkErrorMacro(<< "vtkODBCDatabase::GetTables: Unable to allocate statement");

      return this->Tables;
    }

    status = SQLSetStmtAttr(statement, SQL_ATTR_CURSOR_TYPE,
      static_cast<SQLPOINTER>(SQL_CURSOR_FORWARD_ONLY), SQL_IS_UINTEGER);

    std::string tableType("TABLE,");

    status = SQLTables(statement, nullptr, 0, nullptr, 0, nullptr, 0,
      (SQLCHAR*)(const_cast<char*>(tableType.c_str())), static_cast<SQLSMALLINT>(tableType.size()));

    if (status != SQL_SUCCESS)
    {
      vtkErrorMacro(<< "vtkODBCDatabase::GetTables: Unable to execute table list");
      return this->Tables;
    }

    status = SQLFetchScroll(statement, SQL_FETCH_NEXT, 0);
    while (status == SQL_SUCCESS)
    {
      std::string fieldVal = odbcGetString(statement, 2, -1);
      this->Tables->InsertNextValue(fieldVal);
      status = SQLFetchScroll(statement, SQL_FETCH_NEXT, 0);
    }

    status = SQLFreeHandle(SQL_HANDLE_STMT, statement);
    if (status != SQL_SUCCESS)
    {
      vtkErrorMacro(<< "vtkODBCDatabase::GetTables: Unable to free statement handle.  Error "
                    << status);
    }
    return this->Tables;
  }
}

//------------------------------------------------------------------------------
vtkStringArray* vtkODBCDatabase::GetRecord(const char* table)
{
  this->Record->Reset();
  this->Record->Allocate(20);

  if (!this->IsOpen())
  {
    vtkErrorMacro(<< "GetRecord: Database is not open!");
    return this->Record;
  }

  SQLHANDLE statement;
  SQLRETURN status = SQLAllocHandle(SQL_HANDLE_STMT, this->Internals->Connection, &statement);
  if (status != SQL_SUCCESS)
  {
    vtkErrorMacro(<< "vtkODBCDatabase: Unable to allocate statement: error " << status);
    return this->Record;
  }

  status = SQLSetStmtAttr(
    statement, SQL_ATTR_METADATA_ID, reinterpret_cast<SQLPOINTER>(SQL_TRUE), SQL_IS_INTEGER);

  if (status != SQL_SUCCESS)
  {
    vtkErrorMacro(<< "vtkODBCDatabase::GetRecord: Unable to set SQL_ATTR_METADATA_ID attribute on "
                     "query.  Return code: "
                  << status);
    return nullptr;
  }

  status = SQLSetStmtAttr(statement, SQL_ATTR_CURSOR_TYPE,
    static_cast<SQLPOINTER>(SQL_CURSOR_FORWARD_ONLY), SQL_IS_UINTEGER);

  status = SQLColumns(statement,
    nullptr, // catalog
    0,
    nullptr, // schema
    0, (SQLCHAR*)(const_cast<char*>(table)), static_cast<SQLSMALLINT>(strlen(table)),
    nullptr, // column
    0);

  if (status != SQL_SUCCESS)
  {
    std::string error = GetErrorMessage(SQL_HANDLE_STMT, statement);

    vtkErrorMacro(
      << "vtkODBCDatabase::GetRecord: Unable to retrieve column list (SQLColumns): error "
      << error);
    this->SetLastErrorText(error.c_str());
    SQLFreeHandle(SQL_HANDLE_STMT, statement);
    return this->Record;
  }

  status = SQLFetchScroll(statement, SQL_FETCH_NEXT, 0);
  if (status != SQL_SUCCESS)
  {
    std::string error = GetErrorMessage(SQL_HANDLE_STMT, statement);
    vtkErrorMacro(
      << "vtkODBCDatabase::GetRecord: Unable to retrieve column list (SQLFetchScroll): error "
      << error);
    this->SetLastErrorText(error.c_str());
    SQLFreeHandle(SQL_HANDLE_STMT, statement);
    return this->Record;
  }
  while (status == SQL_SUCCESS)
  {
    std::string fieldName = odbcGetString(statement, 3, -1);
    this->Record->InsertNextValue(fieldName);
    status = SQLFetchScroll(statement, SQL_FETCH_NEXT, 0);
  }

  status = SQLFreeHandle(SQL_HANDLE_STMT, statement);
  if (status != SQL_SUCCESS)
  {
    vtkErrorMacro("vtkODBCDatabase: Unable to free statement handle: error " << status);
  }

  return this->Record;
}

//------------------------------------------------------------------------------
void vtkODBCDatabase::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "DataSourceName: ";
  if (this->DataSourceName == nullptr)
  {
    os << "(none)" << endl;
  }
  else
  {
    os << this->DataSourceName << endl;
  }

  os << indent << "DatabaseName: ";
  if (this->DatabaseName == nullptr)
  {
    os << "(none)" << endl;
  }
  else
  {
    os << this->DatabaseName << endl;
  }

  os << indent << "UserName: ";
  if (this->UserName == nullptr)
  {
    os << "(none)" << endl;
  }
  else
  {
    os << this->UserName << endl;
  }
  os << indent << "HostName: ";
  if (this->HostName == nullptr)
  {
    os << "(none)" << endl;
  }
  else
  {
    os << this->HostName << endl;
  }
  os << indent << "Password: ";
  if (this->Password == nullptr)
  {
    os << "(none)" << endl;
  }
  else
  {
    os << "not displayed for security reason." << endl;
  }
  os << indent << "ServerPort: " << this->ServerPort << endl;

  os << indent << "DatabaseType: " << (this->DatabaseType ? this->DatabaseType : "nullptr") << endl;
}

//------------------------------------------------------------------------------
bool vtkODBCDatabase::HasError()
{
  return this->LastErrorText != nullptr;
}

//------------------------------------------------------------------------------
vtkStdString vtkODBCDatabase::GetURL()
{
  return vtkStdString("GetURL on ODBC databases is not yet implemented");
}

//------------------------------------------------------------------------------
bool vtkODBCDatabase::ParseURL(const char* URL)
{
  std::string urlstr(URL ? URL : "");
  std::string protocol;
  std::string username;
  std::string unused;
  std::string dsname;
  std::string dataport;
  std::string database;

  // Okay now for all the other database types get more detailed info
  if (!vtksys::SystemTools::ParseURL(
        urlstr, protocol, username, unused, dsname, dataport, database))
  {
    vtkErrorMacro("Invalid URL: \"" << urlstr << "\"");
    return false;
  }

  if (protocol == "odbc")
  {
    this->SetUserName(username.c_str());
    this->SetServerPort(atoi(dataport.c_str()));
    this->SetDatabaseName(database.c_str());
    this->SetDataSourceName(dsname.c_str());
    return true;
  }

  return false;
}

//------------------------------------------------------------------------------
vtkStdString vtkODBCDatabase::GetColumnSpecification(
  vtkSQLDatabaseSchema* schema, int tblHandle, int colHandle)
{
  std::ostringstream queryStr;
  queryStr << schema->GetColumnNameFromHandle(tblHandle, colHandle) << " ";

  // Figure out column type
  int colType = schema->GetColumnTypeFromHandle(tblHandle, colHandle);
  std::string colTypeStr;

  switch (static_cast<vtkSQLDatabaseSchema::DatabaseColumnType>(colType))
  {
    case vtkSQLDatabaseSchema::SERIAL:
      colTypeStr = "INTEGER NOT nullptr";
      break;
    case vtkSQLDatabaseSchema::SMALLINT:
      colTypeStr = "SMALLINT";
      break;
    case vtkSQLDatabaseSchema::INTEGER:
      colTypeStr = "INT";
      break;
    case vtkSQLDatabaseSchema::BIGINT:
      colTypeStr = "BIGINT";
      break;
    case vtkSQLDatabaseSchema::VARCHAR:
      colTypeStr = "VARCHAR";
      break;
    case vtkSQLDatabaseSchema::TEXT:
      colTypeStr = "TEXT";
      break;
    case vtkSQLDatabaseSchema::REAL:
      colTypeStr = "FLOAT";
      break;
    case vtkSQLDatabaseSchema::DOUBLE:
      colTypeStr = "DOUBLE PRECISION";
      break;
    case vtkSQLDatabaseSchema::BLOB:
      colTypeStr = "BLOB";
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
  else // if ( !colTypeStr.empty() )
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
      colSizeType = 1;
      break;
    case vtkSQLDatabaseSchema::REAL:
      colSizeType = 0; // Eventually will make DB schemata handle (M,D) sizes
      break;
    case vtkSQLDatabaseSchema::DOUBLE:
      colSizeType = 0; // Eventually will make DB schemata handle (M,D) sizes
      break;
    case vtkSQLDatabaseSchema::BLOB:
      colSizeType = 1;
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

  vtkStdString attStr = schema->GetColumnAttributesFromHandle(tblHandle, colHandle);
  if (!attStr.empty())
  {
    queryStr << " " << attStr;
  }

  return queryStr.str();
}

//------------------------------------------------------------------------------
vtkStdString vtkODBCDatabase::GetIndexSpecification(
  vtkSQLDatabaseSchema* schema, int tblHandle, int idxHandle, bool& skipped)
{
  skipped = false;
  std::string queryStr = ", ";
  bool mustUseName = true;

  int idxType = schema->GetIndexTypeFromHandle(tblHandle, idxHandle);
  switch (idxType)
  {
    case vtkSQLDatabaseSchema::PRIMARY_KEY:
      queryStr += "PRIMARY KEY ";
      mustUseName = false;
      break;
    case vtkSQLDatabaseSchema::UNIQUE:
      queryStr += "UNIQUE ";
      break;
    case vtkSQLDatabaseSchema::INDEX:
      queryStr += "INDEX ";
      break;
    default:
      return {};
  }

  // No index_name for PRIMARY KEYs
  if (mustUseName)
  {
    queryStr += schema->GetIndexNameFromHandle(tblHandle, idxHandle);
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
bool vtkODBCDatabase::CreateDatabase(const char* dbName, bool dropExisting = false)
{
  if (dropExisting)
  {
    this->DropDatabase(dbName);
  }
  std::string queryStr;
  queryStr = "CREATE DATABASE ";
  queryStr += dbName;
  vtkSQLQuery* query = this->GetQueryInstance();
  query->SetQuery(queryStr.c_str());
  bool status = query->Execute();
  query->Delete();
  // Close and re-open in case we deleted and recreated the current database
  this->Close();
  this->Open(this->Password);
  return status;
}

//------------------------------------------------------------------------------
bool vtkODBCDatabase::DropDatabase(const char* dbName)
{
  std::string queryStr;
  queryStr = "DROP DATABASE ";
  queryStr += dbName;
  vtkSQLQuery* query = this->GetQueryInstance();
  query->SetQuery(queryStr.c_str());
  bool status = query->Execute();
  query->Delete();
  return status;
}
VTK_ABI_NAMESPACE_END
