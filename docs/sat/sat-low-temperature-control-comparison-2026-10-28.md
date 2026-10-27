# Low-temperature heating control: a primary-source comparison

Research date: 2026-10-28. Backlog: TASK-1411. Owning component: Smart Thermostat (SAT).

This comparison evaluates control approaches and the evidence needed to choose between them. It does not change SAT, its pinned upstream, or a recorded designer decision. Manufacturer examples establish that an approach exists on the specified equipment; they do not establish that it outperforms SAT. Research results below retain their original equipment, baseline and observation limits.

## 1. Separate the control questions before choosing an algorithm

Three decisions are easily confused:

1. **Required heat:** how much heat the occupied rooms need, given weather, internal gains, schedules and stored heat.
2. **Emitter temperature:** which water-temperature trajectory can deliver that heat through the actual radiators, floor or fan coils.
3. **Appliance operation:** how the boiler or compressor produces it within its capacity range, hydraulic limits, startup sequence and protections.

A room PID answers part of the first question. A heating curve provides a feedforward estimate for the second. A PWM scheduler addresses an actuator that cannot continuously produce sufficiently little heat. None alone proves that the complete installation behaves well.

The practical recommendation is to compare complete control arrangements, with the appliance's own controller and protections included. A comparison between two room-temperature traces alone cannot establish energy efficiency, minimum-capacity operation or compressor protection.

## 2. What comparable commercial controllers actually provide

### 2.1 Weather compensation with optional room influence

Vaillant's sensoCOMFORT VRC 720/3 describes a weather-compensated heating circuit, a heating curve and flow-temperature limits. Room influence can be inactive, active, or expanded; the expanded setting also enables/disables the zone. Adaptive curve adjustment has stated prerequisites. Thus weather feedforward, room correction and demand switching are separate configuration choices in a commercial controller. This is documented functionality, not an independent efficiency comparison. See the [manufacturer instructions, 0020334316_03, sections 2.12 and 2.14.4, printed pp. 8, 13, 17-18](https://www.vaillant.co.uk/documents/controls/sensocomfort/sensocomfort-vrc720%283%29-operating-and-installation-instructions.pdf).

**Decision implication:** weather compensation plus room feedback is an established architecture. Its presence does not validate SAT's particular gains, switching thresholds or duty conversion. Conversely, a custom PID is not necessary merely to combine these two inputs.

### 2.2 Heat-pump water-temperature control with room correction and hydraulic requirements

Daikin's specified Altherma models support weather-dependent leaving-water temperature and optional room-based adjustment of that target. The guide separately defines water-temperature overshoot stopping/restarting the compressor. It requires a guaranteed minimum flow during defrost/backup-heater operation, including when zone valves close. For these particular models it states no minimum water-volume requirement. Water flow and water volume therefore must not be treated as interchangeable requirements. See [4P495248-1, sections 6.4.3 and 8.4, printed pp. 26-27, 54-57 and 63](https://www.daikin.eu/content/dam/document-library/Installer-reference-guide/heat/air-to-water-heat-pump-low-temperature/ERGA04-08DV,DVA_EHVH-D6V,D6VG,D9W,D9WG,DV,DVG_EHVX-D3V,D3VG,D6V,D6VG,D9W,D9WG_4PEN495248-1_2017_12_Installer%20Reference%20Guide_English.pdf).

**Decision implication:** a native appliance controller is a serious baseline. An external controller must establish which target it can request, what the appliance does with that request, and whether external switching disrupts rather than helps its operation. This example does not establish the behaviour of every Daikin product or every heat pump.

### 2.3 Heat-pump demand accumulation and slow emitters

NIBE S1255 offers a heating curve, room-sensor options, compressor-frequency settings and degree-minute thresholds for starting/stopping the compressor or additional heat. The manual warns that a slow system such as underfloor heating may be unsuitable for room-sensor control, and advises waiting 24 hours between curve adjustments. It also identifies a minimum interval between compressor starts in troubleshooting. That troubleshooting wait is not a universal minimum ON-time setting. See [S1255 installer manual, IHB EN 2235-2, menus 1.1, 1.3, 7.1.3 and 7.1.10.3; printed pp. 39-40, 52, 54 and 64](https://installer.nibe.eu/download/18.69d23679185eaf109d92e21/1676544182520/S1255-IHB-631735-2.pdf).

**Decision implication:** room error need not be converted immediately into boiler-style timed pulses. A native controller can accumulate demand and coordinate the compressor. Its exact algorithm and limits must be checked per model; this manual is not proof that a generic degree-minute replacement would improve SAT.

### 2.4 TPI has distinct cycle, minimum ON and minimum OFF settings

Resideo's 2024/2025 catalogue explicitly separates these three TPI parameters. Its application table gives a condensing gas-boiler example below30kW of 6 cycles/hour, 1-minute minimum ON and zero external minimum OFF, versus a heat-pump example of 2 cycles/hour, 20-minute minimum ON and 10-minute minimum OFF. These are manufacturer application recommendations, not universal equipment limits or measured lifetime results. See the [catalogue, printed p. 88](https://www.resideo.com/be/nl/-/media/EMEA%20Site%20Cluster/Belgium%20French/Files/Sales%20Literature/DU-FR-Catalogue-2024_Resideo.pdf).

**Decision implication:** selecting 2 cycles/hour says nothing by itself about whether a 180-second compressor run is acceptable. A scheduler's nominal period, a wear-related minimum runtime and a refrigerant-system restart restriction are different quantities. Obtain the actual appliance's requirements before choosing them.

## 3. What empirical research supports, and what it does not

### 3.1 Rapid cycling can matter, but a gas-boiler number cannot be copied to every compressor

EA Technology's 2012 test-house study used one air-source and one ground-source heat pump, both with fixed-speed compressors. Closing TRVs reduced the active heat-rejection circuit. Runs shorter than about six minutes impaired performance; the report recommends approximately six minutes minimum runtime for the tested class of problem. It discusses solutions with and without a buffer. This is evidence against assuming that any short runtime is harmless, not a universal six-minute prescription for modern inverter units. See the [original DECC report, Summary and sections 7-8](https://assets.publishing.service.gov.uk/government/uploads/system/uploads/attachment_data/file/65695/7389-effects-cycling-heat-pump-performance.pdf).

### 3.2 Field cycling is a useful diagnostic, not a standalone causal efficiency measurement

Brudermueller et al. studied 503 Swiss household heat pumps over 21 months using 15-minute smart-meter data. They derive cycling indicators and identify outliers. The paper explicitly notes that successive short cycles can merge at that resolution, and that DHW responsibility is unknown. It is an observational monitoring study, not a randomized controller comparison; it cannot validate a 180-second hold or prove that fewer starts caused a particular energy saving. See the [original paper, Applied Energy 350 (2023), 121734, sections 5.4 and 6](https://www.uni-bamberg.de/fileadmin/eesys/Publications/1-s2.0-S030626192301098X-main.pdf).

### 3.3 Predictive control can trade electrical efficiency for cheaper heat

Tomás et al. tested MPC in an Offenburg low-energy commercial building with a groundwater heat pump, storage and thermally activated structures. They compare two separate one-week periods: December 2022 conventional operation and February 2023 MPC. They report average operating COP decreasing from 7.58 to 5.96, while heating-energy-specific cost falls from about 2.9 to 1.8 ct/kWh of heat. They align the price series for the comparison and explicitly reject comparing the periods' raw heat totals. The baseline includes a heating-curve-controlled distribution circuit; this is not a clean head-to-head test of every weather-compensation algorithm. Different periods, source conditions and building use limit causal and seasonal claims. See the [original paper, Energies 18 (2025), 1434, sections 2.1, 3 and 4](https://opus.hs-offenburg.de/files/10493/energies-18-01434.pdf).

### 3.4 A residential field demonstration is promising but equipment-specific

Pergantis et al. field-tested predictive supervisory control of an air-to-air heat pump with electric backup in one occupied Indiana house during January-March 2023. The comparison uses 33 MPC days, 11 other test days and historical data; 15 commissioning days are excluded. Relative to the manufacturer's default controller with occupant-selected setpoints, their indoor-outdoor-temperature-normalized estimate is 18.7% daily heating-energy reduction, with a 95% interval of 13.1-24.1%. Comfort was surveyed, but mean indoor conditions were lower under MPC (section5.6), so this is not identical-temperature performance evidence. It is not randomized evidence for hydronic SAT, nor a comparison against a commissioned water-temperature weather curve. Backup-heat management is material. See the [author manuscript, sections 4.4 and 5](https://arxiv.org/html/2402.07032v1).

**Research conclusion:** these studies justify considering predictive supervision when the installation offers useful thermal storage, controllable interfaces and an explicit objective. They do not justify claiming a transferable savings percentage, replacing SAT's low-level controller wholesale, or ranking MPC above a well-commissioned native controller for every home. Cost, electricity, comfort and starts are separate outcomes.

## 4. Physical reasoning that the comparison must retain

The following are first-law deductions and simplified models, not measurements of SAT.

### 4.1 A temperature difference is not a heat meter

For liquid water over an appropriate temperature range:

```text
Qdot_water = massFlow * specificHeat * (Tflow - Treturn)
```

The temperature difference needs a mass-flow measurement or justified estimate. Equal flow temperatures do not imply equal heat output; equal flow-return differences do not imply equal output when pump flow differs. Stored energy also matters:

```text
dStoredEnergy/dt = Qdot_source - Qdot_emitters - Qdot_losses
```

A rising flow temperature can indicate excess source heat, reduced circulation, closed valves, or changing stored energy. It does not uniquely identify low building demand. Commissioning must distinguish these explanations before altering a room-control gain.

### 4.2 Duty is an actuator approximation with assumptions

If the appliance actually supplies approximately constant minimum useful power while ON, then, neglecting startup and storage transients:

```text
meanUsefulHeat approximately dutyFraction * minimumUsefulPower
```

This makes timed low-load operation reasonable when demand is below the minimum continuous output. It does not establish that a linear ratio of water temperatures equals an energy ratio. That mapping needs empirical calibration or a heat-transfer model; a plausible temperature formula is not physical proof.

For an inverter heat pump, first establish its minimum stable output under the actual source/sink conditions and the authority of its native controller. For a gas boiler, establish whether minimum relative modulation is supported and effective. Gas-boiler modulation percentages cannot be assumed to encode compressor speed or minimum heat-pump capacity.

### 4.3 Minimum runtime has a hydraulic and storage cost

A deliberately simplified constant-power model gives:

```text
excessPower = max(0, minimumSourcePower - emitterHeatRejection)
temperatureRise = excessPower * runTime / effectiveHeatCapacity
```

If permissible water-temperature rise and active thermal capacity are small, forcing a longer ON request can raise temperature until the appliance stops itself. Conversely, a larger connected heat sink can absorb a longer run without that rise. A bypass can preserve circulation while returning hot water rapidly; it does not necessarily provide useful heat rejection or enough storage. A buffer can change storage and hydraulic separation, but introduces its own mixing, temperature and standing-loss tradeoffs. No blanket buffer recommendation follows.

The active water volume, open emitter area, floor/slab coupling, flow and appliance conditions belong in the assessment. A room sensor's slow response cannot reveal all of these fast appliance-side constraints.

## 5. How the OpenTherm application document constrains this comparison

The local *OpenTherm Application Functional Specification v1.1f*, printed pp. 6-10, distinguishes temperature control from low-load minimum-capacity ON/OFF operation. Detailed control recipes sit under section 2.1.1.2's suggestion heading. Section 2.1.1.2.10 separately requires reading ID3 at startup and honoring the slave's HB4 low-load prohibition. A preferred room algorithm cannot override that interoperability requirement.

The without-ID14 recipe raises the setpoint enough to ignite before tracking actual flow or slightly below it. The with-ID14 recipe separately limits relative modulation. Pump overrun is allowed, and boiler anti-cycle delay can distort requested duty. Therefore compare measured boiler-facing commands and flame/compressor operation, not only the room controller's intended phase. Source: `D:/Users/Robert/Documents/GitHub/RvdB/OTGW-other-projects/OT-Spec/OT Application Functional Specification v1-1f.pdf`, sections 2.1.1.2.3-2.1.1.5. The associated markdown is a searchable extraction; use the PDF when a table or diagram is ambiguous.

## 6. Decision criteria for the SAT review

| Candidate arrangement | When it deserves a trial | What would disqualify or weaken the choice |
|---|---|---|
| Commissioned native weather compensation, optionally with bounded room influence | The appliance exposes an appropriate native control path and can meet comfort at low water temperatures. | Poorly matched curve, unrepresentative reference room, unsuitable emitter response, or unavailable control interface. |
| Room-based load compensation requesting water temperature | Room feedback improves recovery or handles changing gains, with adequate sensor placement and bounded requests. | Repeated high-temperature recovery, room oscillation, or interaction with native limits that is not observed. |
| Boiler low-load timed minimum-capacity control | Demand is below verified minimum continuous output and the slave permits master low-load control. | Ineffective minimum-capacity control, failed ignition, too-short actual burns, or a slave veto. |
| External heat-pump timed demand | The equipment interface and manual support it, native protections remain authoritative, and real cycle behaviour is measured. | Treating generic ON time as compressor protection, interrupting useful modulation, defrost or DHW, or inadequate hydraulics. |
| Predictive supervisory scheduling over a commissioned low-level controller | Forecasts, storage, controllable interfaces and the chosen energy/cost/comfort objective offer measurable value. | No reliable model or fallback, unavailable measurements, or gains that vanish against a properly commissioned baseline. |

These are trial-selection criteria, not an implementation decision. Changing SAT's controller structure, dependency set, protocol contract or accepted architectural decision requires the repository's normal ADR and human review process. Recorded SAT decisions must be confronted explicitly; this research does not silently supersede them.

## 7. A fair measurement protocol

1. Record the exact appliance, firmware, controller version, source/emitter arrangement, curve, limits, valve state and sensor placement. Establish which controller owns each target and protection.
2. Establish a commissioned baseline before testing an alternative. Keep room schedules, DHW treatment and measurement boundaries comparable; report weather and occupancy differences.
3. Log requested and actual boiler/compressor state separately. Capture ignition delay, actual ON/OFF duration distributions, failed starts, native lockouts, defrost/DHW interruptions, flow, return and circulation.
4. Measure delivered heat and purchased energy with defined boundaries. Report room error and occupied discomfort separately from gas/electricity, cost, COP and starts. Do not calculate seasonal efficiency from a handful of instantaneous COP samples.
5. Include mild-load operation, normal load and setpoint recovery. Test changes in open emitter area and post-DHW recovery. For inertial floors, allow the building to settle before comparing settings.
6. Prefer repeated alternating or matched-condition runs; predeclare how commissioning periods, missing sensors and unusual occupancy are handled. If conditions cannot be matched, quantify the limitation instead of inventing a normalized saving.

A trace showing fewer starts but higher water temperatures is an unresolved tradeoff until comfort and energy are measured. A lower electricity bill is not proof of higher COP, as the Offenburg example illustrates. A stable room graph is not proof of correct OpenTherm interoperability.

## 8. Source ledger and scope

All eight sources below were opened/reviewed online on 2026-10-28. Manufacturer statements describe the named product/document revision. No marketing savings percentage is used. The research entries are original reports or papers; they are not pooled into a common effect size.

| ID | Primary source | Evidence used and scope |
|---|---|---|
| M1 | [Vaillant VRC 720/3, 0020334316_03, 2024](https://www.vaillant.co.uk/documents/controls/sensocomfort/sensocomfort-vrc720%283%29-operating-and-installation-instructions.pdf) | Heating curve, optional room influence, target limits; product instructions. |
| M2 | [Daikin Altherma, 4P495248-1, 2017](https://www.daikin.eu/content/dam/document-library/Installer-reference-guide/heat/air-to-water-heat-pump-low-temperature/ERGA04-08DV,DVA_EHVH-D6V,D6VG,D9W,D9WG,DV,DVG_EHVX-D3V,D3VG,D6V,D6VG,D9W,D9WG_4PEN495248-1_2017_12_Installer%20Reference%20Guide_English.pdf) | LWT/room control, overshoot, hydraulic requirements; listed models only. |
| M3 | [NIBE S1255, IHB EN 2235-2](https://installer.nibe.eu/download/18.69d23679185eaf109d92e21/1676544182520/S1255-IHB-631735-2.pdf) | Degree minutes, frequency settings and floor response; model-specific manual. |
| M4 | [Resideo catalogue 2024/2025, p. 88](https://www.resideo.com/be/nl/-/media/EMEA%20Site%20Cluster/Belgium%20French/Files/Sales%20Literature/DU-FR-Catalogue-2024_Resideo.pdf) | TPI application recommendations; relevant table visually checked. |
| R1 | [Green/EA Technology, 2012](https://assets.publishing.service.gov.uk/government/uploads/system/uploads/attachment_data/file/65695/7389-effects-cycling-heat-pump-performance.pdf) | Test-house cycling experiment; two fixed-speed units. |
| R2 | [Brudermueller et al., 2023](https://www.uni-bamberg.de/fileadmin/eesys/Publications/1-s2.0-S030626192301098X-main.pdf) | Observational cycling diagnostics; 15-minute resolution limitation. |
| R3 | [Tomas et al., 2025, university full text](https://opus.hs-offenburg.de/files/10493/energies-18-01434.pdf) | Commercial MPC field case; Figure 9 and printed p. 15 visually checked. |
| R4 | [Pergantis et al., 2024, author manuscript](https://arxiv.org/html/2402.07032v1) | Residential predictive-control field demonstration; air-to-air system. |

The online manuals are deliberately revision-specific examples, not a claim that these are the newest instructions for an unidentified installation. These eight sources do not report a controlled head-to-head test of SAT against the commercial controllers. They therefore leave that comparison unproven and establish neither superiority nor inferiority of SAT.
