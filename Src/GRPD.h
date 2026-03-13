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

#include "PDSimulater.h"

void Start();

// Initializes the provided simulation model context with mesh data
void Model(GRPD::Core::PDSimulater &model);

void Material(GRPD::Core::PDSimulater &model);

void Initialize(GRPD::Core::PDSimulater &model);

void Solve(GRPD::Core::PDSimulater &model);

void Write(const GRPD::Core::PDSimulater &model);

#endif
