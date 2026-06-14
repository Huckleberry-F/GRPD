#!/bin/bash

# ==============================================================================
# GRPD One-Click Setup & Build Routing Script
# ==============================================================================

if [[ "$OSTYPE" == "darwin"* ]]; then
    chmod +x ./setup/mac/setup_and_build.sh
    exec ./setup/mac/setup_and_build.sh
else
    chmod +x ./setup/linux/setup_and_build.sh
    exec ./setup/linux/setup_and_build.sh
fi

