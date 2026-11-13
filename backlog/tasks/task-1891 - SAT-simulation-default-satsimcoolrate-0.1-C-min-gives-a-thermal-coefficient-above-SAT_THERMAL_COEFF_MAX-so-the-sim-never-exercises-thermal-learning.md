---
id: TASK-1891
title: >-
  SAT simulation: default satsimcoolrate 0.1 C/min gives a thermal coefficient
  above SAT_THERMAL_COEFF_MAX, so the sim never exercises thermal learning
status: To Do
assignee: []
created_date: '2026-11-13T08:27:00Z'
updated_date: '2026-11-13T08:43:00Z'
labels:
  - sat
  - simulation
dependencies: []
priority: medium
ordinal: 959000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by otgw-bench-tester (09) on the TASK-1869 bench of beta.220 (.61). fSimCoolRate defaults to 0.1 C/min at SAT_SIM_COOL_REF_K 10 K (SATtypes.h:558, SATcontrol.ino:185, :5841), about 0.6 C/h per K, while satUpdateThermalLearning() accepts samples only up to SAT_THERMAL_COEFF_MAX 0.3 (SATcontrol.ino:158, :6683). With the default every sample is rejected, so a sim run never tests the thermal model; 09 had to lower the cool rate. Decide: lower the sim default, or document the recipe. Simulation only, no effect on real control.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 The default sim produces accepted thermal samples (host test or bench log), or the docs name the cool rate the sim needs
<!-- AC:END -->
