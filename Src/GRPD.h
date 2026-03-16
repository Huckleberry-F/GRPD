/**************************************************************************************************************
 *                                                                                                             *
 *                                                  GRPD.h
 *                               *
 *                                                                                                             *
 *-------------------------------------------------------------------------------------------------------------*
 *                                                                                                             *
 *                       By Huckelberry Zhang  2026-03-06  zhanghanbo@inspur.com
 *                           *
 *                                                                                                             *
 **************************************************************************************************************/
#ifndef GRPD_H
#define GRPD_H

#include "PDContext.h"

void Start();

// Initializes the provided simulation model context with mesh data
void Model(PDCommon::Core::PDContext &model);

void Material(PDCommon::Core::PDContext &model);

void Initialize(PDCommon::Core::PDContext &model);

void Solve(PDCommon::Core::PDContext &model);

void Write(const PDCommon::Core::PDContext &model);

#endif
