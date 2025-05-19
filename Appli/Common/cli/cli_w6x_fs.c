#include "w6x_fs.h"
#include "cli.h"
#include "cli_prv.h"

#include <string.h>

static void prvW6XFSCommand( ConsoleIO_t * const pxCIO,
                               uint32_t ulArgc,
                               char * ppcArgv[] );

const CLI_Command_Definition_t xCommandDef_w6x_fs =
{
    .pcCommand            = "w6x_fs",
    .pcHelpString         =
        "w6x_fs:\r\n"
        "    Perform fs operations on W6X.\r\n"
        "    Usage:\r\n"
        "    w6x_fs ls\r\n"
        "        List files\r\n\n"
        "    w6x_fs rm <file_name>\r\n"
        "        Delete a file\r\n\n",
    prvW6XFSCommand
};

#define OPERATION_ARG_INDEX  1
#define FILE_NAME_INDEX      2

void w6x_ls(ConsoleIO_t *const pxCIO)
{
  W6X_Status_t xStatus = W6X_STATUS_ERROR;
  W6X_FS_FilesListFull_t *files_list = NULL;
  W61_Object_t *pLfsCtx = NULL;

  W6X_FileInit();

  pLfsCtx = W6X_GetDefaultFsCtx();

  xStatus = W6X_file_list(pLfsCtx, &files_list);

  if (xStatus == W6X_STATUS_OK)
  {
    static char formattedString[128];
    memset(formattedString, 0, 128);

    for (uint32_t index = 0; index < files_list->ncp_files_list.nb_files; index++)
    {
      snprintf(formattedString, sizeof(formattedString), "%s\r\n", files_list->ncp_files_list.filename[index]);
      pxCIO->print(formattedString);
    }
  }
}

void w6x_rm(ConsoleIO_t *const pxCIO, const char *pFileName)
{
  W6X_Status_t xStatus = W6X_STATUS_ERROR;
  uint32_t uFileSize = 0;
  W61_Object_t *pLfsCtx = NULL;

  W6X_FileInit();

  pLfsCtx = W6X_GetDefaultFsCtx();

  xStatus = W6X_file_stat(pLfsCtx, pFileName, &uFileSize);

  if (xStatus == W6X_STATUS_OK)
  {
    xStatus = W6X_file_delete(pLfsCtx, pFileName);

    if (xStatus == W6X_STATUS_OK)
    {
      pxCIO->print("File deleted\r\n");
    }
    else
    {
      pxCIO->print("Failed to delete file\r\n");
    }
  }
  else
  {
    pxCIO->print("File not found\r\n");
  }
}

static void prvW6XFSCommand( ConsoleIO_t * const pxCIO,
                               uint32_t ulArgc,
                               char * ppcArgv[] )
{
  const char *pcVerb = NULL;

  if (ulArgc <= OPERATION_ARG_INDEX)
  {
    return;
  }

  pcVerb = ppcArgv[ OPERATION_ARG_INDEX];

  if (0 == strcmp("ls", pcVerb))
  {
    w6x_ls(pxCIO);
  }
  else if (0 == strcmp("rm", pcVerb))
  {
    if (ulArgc > FILE_NAME_INDEX)
    {
      const char *pFileName = ppcArgv[ FILE_NAME_INDEX];

      w6x_rm(pxCIO, pFileName);
    }
  }
  else
  {
      pxCIO->print( xCommandDef_w6x_fs.pcHelpString );
  }
}
