/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_INCLUDE_COMMON_ERRORCODES_H
#define COEUS_INCLUDE_COMMON_ERRORCODES_H

#include "ErrorDefinition.h"

using namespace coeus::common;

const ErrorCode SUCCESSFUL = {0, "SUCCESSFUL"};

// Errors
const ErrorCode HERMES_CONNECT_FAILED = {1000, "Connecting to Hermes failed"};
const ErrorCode NO_JSON = {2000, "Could not find JSON file with operators"};
const ErrorCode BAD_JSON = {2001, "Submitted json is badly formatted"};

#endif //COEUS_INCLUDE_COMMON_ERRORCODES_H
