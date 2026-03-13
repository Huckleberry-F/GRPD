// ============================================================================
// Start.cpp - Start GRPD exe
// Responsibility: Show Logo and print the initial information
// ============================================================================
#include "GRPD.h"
#include "Logger.h"

void Start() {

  LOG_SET_FILE("../../Output/log.txt");

  SHOW_LOGO();

  LOG_INFO("GRPD is running...!");
}
