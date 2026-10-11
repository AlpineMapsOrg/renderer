### avaKot (reference avaKotNullTest: 11.7 ha, max pfv 40 m/s, com1DFA run time 118 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | +469 / +474 | 25.1 (31.0) | 0.99 / 0.13 | 34.3 | 81 | – | – / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | +575 / +575 | 24.1 (31.0) | 0.99 / 0.10 | 81.3 | 86 | – | 1 / – | – |
| MPM Stomakhin 2013, Coulomb | +8 / +8 | 30.9 (31.0) | 0.68 / 0.75 | 12.5 | 54 | 80 | 5 / 3 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | ≥+598 / +589 | 24.0 (31.0) | 0.65 / 0.14 | 38.3 | 83 | 73 | 5 / 3 | 0.00 % |
| MPM Drucker-Prager, Coulomb | +532 / +247 | 27.7 (31.0) | 0.95 / 0.37 | 28.1 | 113 | 88 | 53 / 34 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+598 / +512 | 24.8 (31.0) | 0.75 / 0.10 | 51.0 | 116 | 61 | 42 / 30 | 0.06 % |
| MPM Cam Clay (Li V), Coulomb | +213 / +164 | 28.6 (31.0) | 0.98 / 0.35 | 20.7 | 100 | 89 | 43 / 25 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | ≥+598 / +455 | 25.4 (31.0) | 0.74 / 0.10 | 41.3 | 108 | 60 | 34 / 23 | 0.00 % |

### avaGar (reference avaGarNullTest: 28.2 ha, max pfv 50 m/s, com1DFA run time 122 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | +200 / +200 | 26.3 (29.3) | 0.91 / 0.31 | 31.5 | 78 | – | 3159 / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | ≥+294 / ≥+297 | 24.5 (29.3) | 0.97 / 0.17 | 59.7 | 73 | – | 0 / – | – |
| MPM Stomakhin 2013, Coulomb | -187 / -182 | 31.2 (29.3) | 0.10 / 1.00 | 17.3 | 54 | 231 | 101 / 56 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | +286 / +275 | 24.9 (29.3) | 0.83 / 0.34 | 35.8 | 76 | 85 | 36 / 22 | 0.00 % |
| MPM Drucker-Prager, Coulomb | +256 / +222 | 25.9 (29.3) | 0.98 / 0.31 | 41.2 | 96 | 111 | 58 / 36 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+297 / +290 | 24.6 (29.3) | 1.00 / 0.17 | 70.6 | 105 | 76 | 43 / 28 | 0.01 % |
| MPM Cam Clay (Li V), Coulomb | +132 / +87 | 28.2 (29.3) | 0.95 / 0.46 | 32.3 | 113 | 107 | 51 / 32 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | ≥+297 / ≥+297 | 24.5 (29.3) | 1.00 / 0.17 | 72.9 | 101 | 78 | 42 / 27 | 0.10 % |

### avaHit (reference avaHitNullTest: 120.8 ha, max pfv 53 m/s, com1DFA run time 154 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | +91 / +96 | 26.2 (28.2) | 0.99 / 0.47 | 146.9 | 75 | – | 8789 / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | +196 / +206 | 23.8 (28.2) | 0.94 / 0.37 | 151.3 | 76 | – | 1 / – | – |
| MPM Stomakhin 2013, Coulomb | -120 / -110 | 29.7 (28.2) | 0.81 / 0.84 | 129.0 | 57 | 151 | 103 / 67 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | ≥+220 / ≥+225 | 23.3 (28.2) | 1.00 / 0.34 | 174.3 | 104 | 70 | 43 / 23 | 0.00 % |
| MPM Drucker-Prager, Coulomb | ≥+220 / +201 | 24.0 (28.2) | 1.00 / 0.36 | 175.6 | 131 | 83 | 60 / 33 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+220 / ≥+225 | 23.3 (28.2) | 1.00 / 0.27 | 206.3 | 105 | 66 | 48 / 26 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb | +57 / +67 | 26.7 (28.2) | 1.00 / 0.52 | 147.6 | 112 | 90 | 61 / 34 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | ≥+220 / ≥+225 | 23.3 (28.2) | 1.00 / 0.30 | 183.6 | 96 | 68 | 45 / 23 | 0.00 % |

### avaWog (reference avaWogNullTest: 54.3 ha, max pfv 46 m/s, com1DFA run time 163 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | -49 / -44 | 26.0 (25.5) | 0.34 / 0.97 | 49.2 | 58 | – | 657 / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | +34 / +44 | 25.0 (25.5) | 0.97 / 0.67 | 72.8 | 62 | – | 1 / – | – |
| MPM Stomakhin 2013, Coulomb | -975 / -1000 | 31.9 (25.5) | 0.00 / – | 34.4 | 45 | 51 | 27 / 15 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | +84 / +84 | 24.6 (25.5) | 0.58 / 0.60 | 53.7 | 82 | 66 | 32 / 14 | 0.00 % |
| MPM Drucker-Prager, Coulomb | +187 / +103 | 24.4 (25.5) | 0.98 / 0.54 | 86.0 | 92 | 79 | 46 / 42 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+439 / +375 | 21.0 (25.5) | 1.00 / 0.19 | 150.0 | 106 | 66 | 42 / 27 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb | +34 / -54 | 26.2 (25.5) | 0.57 / 0.89 | 57.5 | 98 | 70 | 35 / 23 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | +389 / +232 | 23.1 (25.5) | 1.00 / 0.24 | 127.1 | 113 | 65 | 36 / 22 | 0.00 % |

### avaMal (reference avaMalNullTest: 133.3 ha, max pfv 53 m/s, com1DFA run time 174 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | -119 / -115 | 26.6 (25.4) | 0.28 / 1.00 | 110.6 | 64 | – | 4760 / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | -27 / -22 | 25.6 (25.4) | 0.84 / 0.36 | 148.8 | 67 | – | 1 / – | – |
| MPM Stomakhin 2013, Coulomb | -779 / -775 | 29.0 (25.4) | 0.00 / – | 92.7 | 56 | 57 | 29 / 17 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | +164 / +160 | 23.9 (25.4) | 0.87 / 0.38 | 188.4 | 90 | 106 | 48 / 28 | 0.00 % |
| MPM Drucker-Prager, Coulomb | +164 / +115 | 24.3 (25.4) | 0.48 / 0.21 | 192.4 | 102 | 92 | 48 / 30 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+231 / ≥+231 | 23.5 (25.4) | 1.00 / 0.13 | 300.3 | 98 | 108 | 54 / 32 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb | -4 / -303 | 27.5 (25.4) | 0.02 / 1.00 | 138.2 | 134 | 92 | 37 / 22 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | ≥+231 / ≥+231 | 23.5 (25.4) | 1.00 / 0.16 | 258.5 | 128 | 130 | 55 / 35 | 0.00 % |

### All five avalanches (pft > 0.1 m)

| model | datasets | mean abs. runout difference [m] | of them at the path end (lower bound) | mean abs. runout angle difference [°] | mean recall / precision | max pfv range [m/s] |
|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | 5 | 186 | 0 | 2.5 | 0.70 / 0.58 | 58–81 |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | 5 | 229 | 1 | 3.4 | 0.94 / 0.33 | 62–86 |
| MPM Stomakhin 2013, Coulomb | 5 | 415 | 0 | 2.7 | 0.32 / 0.87 | 45–57 |
| MPM Stomakhin 2013, Voellmy | 5 | 266 | 1 | 3.8 | 0.79 / 0.36 | 76–104 |
| MPM Drucker-Prager, Coulomb | 5 | 178 | 0 | 2.6 | 0.88 / 0.36 | 92–131 |
| MPM Drucker-Prager, Voellmy | 5 | 326 | 2 | 4.5 | 0.95 / 0.17 | 98–116 |
| MPM Cam Clay (Li V), Coulomb | 5 | 135 | 0 | 1.5 | 0.71 / 0.64 | 98–134 |
| MPM Cam Clay (Li V), Voellmy | 5 | 288 | 3 | 4.0 | 0.95 / 0.19 | 96–128 |
