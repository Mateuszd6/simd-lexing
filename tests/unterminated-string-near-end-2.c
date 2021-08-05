SQLITE_PRIVATE int sqlite3StmtVtabInit(sqlite3 *db){
  int rc = SQLITE_OK;
  rc = sqlite3_create_module(db, "sqlite_stmt", &stmtModule, 0);
  return rc;
}

__declspec(dllexport)
SQLITE_API int sqlite3_stmt_init(
  sqlite3 *db,
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  int rc = SQLITE_OK;
  SQLITE_EXTENSION_INIT2(pApi);
  rc = sqlite3StmtVtabInit(db);
  return rc;
}

/************** End of stmt.c ************************************************/
/* Return the source-id for this library */
// SQLITE_API const char *sqlite3_sourceid(void){ return SQLITE_API const char *sqlite3_sourceid(void){ return SQLITE_API const char *sqlite3_sourceid(void){
return_SQLITE_API
const char *sqlite3_sourceid(void){ return_SQLITE_API const char " sqqqqte3_sourceid(void){ return qoiej qweoijqweo jqw eoiqj weoiqj eoiqjw         "SQLITE_SOURCE_ID; } "hello char
