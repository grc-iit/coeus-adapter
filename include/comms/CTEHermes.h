/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Coeus-adapter. The full Coeus-adapter copyright      *
 * notice, including terms governing use, modification, and redistribution,  *
 * is contained in the COPYING file, which can be found at the top directory.*
 * If you do not have access to the file, you may request a copy             *
 * from scslab@iit.edu.                                                      *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef COEUS_INCLUDE_COMMS_CTEHERMES_H_
#define COEUS_INCLUDE_COMMS_CTEHERMES_H_

#include "interfaces/IHermes.h"
#include "CTETagClient.h"

#include <wrp_cte/core/core_client.h>
#include <wrp_cte/core/core_tasks.h>
#include <chimaera/chimaera.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace coeus {

/**
 * CTEHermes: CTE-based implementation of IHermes interface.
 *
 * Inherits wrp_cte::core::Client directly so it is a CTE client; also
 * implements IHermes (connect, GetTag, Demote, Prefetch, tag) for the
 * Hermes engine. All CTE operations use this Client base.
 */
class CTEHermes : public IHermes, public wrp_cte::core::Client {
 public:
  /**
   * Constructor
   */
  CTEHermes();

  /**
   * Destructor
   */
  ~CTEHermes() override;

  /**
   * Connect/Initialize CTE (if not already initialized)
   * @return true if successful, false otherwise
   */
  bool connect() override;

  /**
   * Get or create a tag by name
   * Creates an ITag wrapper that uses CTE client for operations
   * @param tag_name Name of the tag
   * @return true if successful, false otherwise
   */
  bool GetTag(const std::string &tag_name) override;

  /**
   * Demote blob (CTE handles automatically via scoring)
   * @param tag_name Name of the tag
   * @param blob_name Name of the blob
   * @return true (no-op, CTE handles automatically)
   */
  bool Demote(const std::string &tag_name, const std::string &blob_name) override;

  /**
   * Prefetch blob (CTE handles automatically via scoring)
   * @param tag_name Name of the tag
   * @param blob_name Name of the blob
   * @return true (no-op, CTE handles automatically)
   */
  bool Prefetch(const std::string &tag_name, const std::string &blob_name) override;

 private:
  bool is_connected_ = false;  // Whether connect() has been called successfully
};

} // namespace coeus

#endif // COEUS_INCLUDE_COMMS_CTEHERMES_H_

