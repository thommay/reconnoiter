/*
 * Copyright (c) 2009, OmniTI Computer Consulting, Inc.
 * All rights reserved.
 * The software in this package is published under the terms of the GPL license
 * a copy of which can be found at:
 * https://labs.omniti.com/reconnoiter/trunk/src/java/LICENSE
 */

package com.omniti.reconnoiter.event;

import java.util.UUID;
import com.omniti.reconnoiter.StratconMessage;
import com.omniti.reconnoiter.EventHandler;

public class StratconQueryStop extends StratconQueryBase {

  public String getPrefix() {
    return "q";
  }

  /*  'q' REMOTE ID */
  public StratconQueryStop() {}
  public StratconQueryStop(String[] parts) throws Exception {
    super(parts);
    uuid = UUID.fromString(parts[2]);
  }
  public UUID getUUID() {
    return uuid;
  }

  public int numparts() { return 3; }

  public void handle(EventHandler eh) {
    eh.deregisterQuery(getUUID());
  }
}

