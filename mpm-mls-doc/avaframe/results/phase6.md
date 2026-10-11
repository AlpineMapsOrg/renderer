### avaKot (reference avaKotNullTest: 11.7 ha, max pfv 40 m/s, com1DFA run time 118 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | +469 / +474 | 25.1 (31.0) | 0.99 / 0.13 | 34.3 | 81 | – | – / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | +575 / +575 | 24.1 (31.0) | 0.99 / 0.10 | 81.3 | 86 | – | 1 / – | – |
| MPM Stomakhin 2013, Coulomb | +8 / +8 | 30.9 (31.0) | 0.67 / 0.75 | 12.5 | 54 | 80 | 3 / 2 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | ≥+598 / ≥+598 | 23.9 (31.0) | 0.62 / 0.14 | 36.1 | 81 | 76 | 3 / 2 | 0.00 % |
| MPM Drucker-Prager, Coulomb | +532 / +247 | 27.7 (31.0) | 0.95 / 0.37 | 28.2 | 113 | 89 | 35 / 24 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+598 / +512 | 24.8 (31.0) | 0.75 / 0.10 | 51.0 | 116 | 61 | 28 / 32 | 0.06 % |
| MPM Cam Clay (Li V), Coulomb | +213 / +155 | 28.8 (31.0) | 0.99 / 0.35 | 20.3 | 113 | 88 | 31 / 25 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | ≥+598 / +469 | 25.2 (31.0) | 0.68 / 0.09 | 40.6 | 111 | 60 | 24 / 23 | 0.01 % |
| MPM Cam Clay (Li I cold dense), Coulomb | +296 / +189 | 28.3 (31.0) | 0.94 / 0.29 | 30.4 | 125 | 85 | 28 / 26 | 0.00 % |
| MPM Cam Clay (Li II warm shear), Coulomb | +125 / -40 | 31.6 (31.0) | 0.48 / 0.75 | 13.3 | 75 | 91 | 25 / 20 | 0.00 % |
| MPM Cam Clay (Li III sliding slab), Coulomb | -72 / -63 | 31.9 (31.0) | 0.47 / 0.85 | 13.1 | 52 | 83 | 18 / 15 | 0.00 % |
| MPM Cam Clay (Li IV warm plug), Coulomb | -690 / -690 | 36.6 (31.0) | 0.00 / – | 6.8 | 44 | 56 | 12 / 9 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.40 | +426 / +271 | 27.4 (31.0) | 0.81 / 0.22 | 23.9 | 106 | 74 | 22 / 19 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.47 | +232 / +169 | 28.6 (31.0) | 0.94 / 0.31 | 21.2 | 107 | 84 | 28 / 20 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.55 | +159 / +77 | 29.9 (31.0) | 0.97 / 0.54 | 15.9 | 107 | 100 | 34 / 25 | 0.00 % |

### avaGar (reference avaGarNullTest: 28.2 ha, max pfv 50 m/s, com1DFA run time 122 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | +200 / +200 | 26.3 (29.3) | 0.91 / 0.31 | 31.5 | 78 | – | 3159 / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | ≥+294 / ≥+297 | 24.5 (29.3) | 0.97 / 0.17 | 59.7 | 73 | – | 0 / – | – |
| MPM Stomakhin 2013, Coulomb | -99 / -104 | 30.5 (29.3) | 0.22 / 1.00 | 19.9 | 50 | 117 | 5 / 3 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | ≥+297 / ≥+297 | 24.5 (29.3) | 1.00 / 0.31 | 47.9 | 75 | 92 | 4 / 3 | 0.52 % |
| MPM Drucker-Prager, Coulomb | +256 / +218 | 26.0 (29.3) | 0.98 / 0.31 | 41.5 | 101 | 111 | 42 / 27 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+297 / ≥+294 | 24.6 (29.3) | 1.00 / 0.17 | 70.4 | 100 | 76 | 29 / 20 | 0.01 % |
| MPM Cam Clay (Li V), Coulomb | +158 / +109 | 27.9 (29.3) | 0.95 / 0.45 | 32.2 | 126 | 108 | 40 / 27 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | ≥+297 / ≥+297 | 24.5 (29.3) | 1.00 / 0.17 | 70.7 | 101 | 79 | 27 / 22 | 0.10 % |
| MPM Cam Clay (Li I cold dense), Coulomb | +237 / +128 | 27.6 (29.3) | 0.97 / 0.38 | 35.9 | 98 | 109 | 30 / 25 | 0.00 % |
| MPM Cam Clay (Li II warm shear), Coulomb | -114 / -148 | 30.8 (29.3) | 0.15 / 1.00 | 20.4 | 56 | 171 | 52 / 33 | 0.00 % |
| MPM Cam Clay (Li III sliding slab), Coulomb | -153 / -173 | 31.1 (29.3) | 0.08 / 1.00 | 20.1 | 45 | 368 | 124 / 73 | 0.00 % |
| MPM Cam Clay (Li IV warm plug), Coulomb | -227 / -296 | 32.1 (29.3) | 0.00 / – | 18.2 | 47 | 251 | 66 / 43 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.40 | +215 / +188 | 26.5 (29.3) | 0.95 / 0.28 | 40.9 | 121 | 94 | 35 / 24 | 0.01 % |
| MPM Cam Clay (Li V), Coulomb mu 0.47 | +226 / +109 | 27.9 (29.3) | 0.98 / 0.41 | 33.4 | 120 | 106 | 39 / 28 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.55 | +87 / +19 | 29.1 (29.3) | 0.84 / 0.72 | 27.0 | 110 | 111 | 39 / 26 | 0.00 % |

### avaHit (reference avaHitNullTest: 120.8 ha, max pfv 53 m/s, com1DFA run time 154 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | +91 / +96 | 26.2 (28.2) | 0.99 / 0.47 | 146.9 | 75 | – | 8789 / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | +196 / +206 | 23.8 (28.2) | 0.94 / 0.37 | 151.3 | 76 | – | 1 / – | – |
| MPM Stomakhin 2013, Coulomb | -81 / -77 | 29.2 (28.2) | 0.89 / 0.83 | 132.1 | 60 | 88 | 6 / 4 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | ≥+220 / ≥+225 | 23.3 (28.2) | 1.00 / 0.37 | 168.1 | 88 | 73 | 5 / 3 | 0.00 % |
| MPM Drucker-Prager, Coulomb | ≥+215 / +187 | 24.4 (28.2) | 1.00 / 0.36 | 175.9 | 132 | 83 | 40 / 25 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+220 / ≥+225 | 23.3 (28.2) | 1.00 / 0.27 | 206.2 | 105 | 67 | 32 / 18 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb | +62 / +67 | 26.7 (28.2) | 1.00 / 0.51 | 148.7 | 109 | 90 | 45 / 27 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | ≥+220 / ≥+225 | 23.3 (28.2) | 1.00 / 0.30 | 184.2 | 102 | 68 | 34 / 19 | 0.00 % |
| MPM Cam Clay (Li I cold dense), Coulomb | +105 / +77 | 26.5 (28.2) | 1.00 / 0.43 | 164.3 | 109 | 84 | 39 / 23 | 0.00 % |
| MPM Cam Clay (Li II warm shear), Coulomb | -10 / -14 | 28.5 (28.2) | 0.95 / 0.75 | 138.8 | 58 | 114 | 56 / 37 | 0.00 % |
| MPM Cam Clay (Li III sliding slab), Coulomb | -62 / -57 | 29.0 (28.2) | 0.92 / 0.79 | 134.7 | 55 | 148 | 65 / 44 | 0.00 % |
| MPM Cam Clay (Li IV warm plug), Coulomb | -57 / -48 | 28.8 (28.2) | 0.79 / 0.92 | 108.3 | 57 | 80 | 30 / 20 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.40 | +163 / +134 | 25.5 (28.2) | 1.00 / 0.42 | 159.2 | 122 | 78 | 38 / 21 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.47 | +110 / +81 | 26.4 (28.2) | 1.00 / 0.49 | 151.3 | 110 | 88 | 44 / 26 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.55 | +24 / +29 | 27.4 (28.2) | 0.97 / 0.63 | 140.1 | 97 | 90 | 47 / 30 | 0.00 % |

### avaWog (reference avaWogNullTest: 54.3 ha, max pfv 46 m/s, com1DFA run time 163 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | -49 / -44 | 26.0 (25.5) | 0.34 / 0.97 | 49.2 | 58 | – | 657 / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | +34 / +44 | 25.0 (25.5) | 0.97 / 0.67 | 72.8 | 62 | – | 1 / – | – |
| MPM Stomakhin 2013, Coulomb | -906 / -901 | 31.0 (25.5) | 0.00 / – | 39.0 | 51 | 54 | 3 / 2 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | +301 / +306 | 21.8 (25.5) | 0.66 / 0.45 | 62.5 | 76 | 74 | 3 / 2 | 0.00 % |
| MPM Drucker-Prager, Coulomb | +375 / +103 | 24.4 (25.5) | 0.99 / 0.51 | 86.8 | 104 | 80 | 31 / 20 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+439 / +370 | 21.1 (25.5) | 1.00 / 0.19 | 149.3 | 103 | 66 | 28 / 18 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb | +84 / -89 | 26.7 (25.5) | 0.55 / 0.89 | 57.5 | 97 | 70 | 24 / 16 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | +375 / +246 | 22.9 (25.5) | 1.00 / 0.24 | 128.0 | 127 | 65 | 26 / 17 | 0.00 % |
| MPM Cam Clay (Li I cold dense), Coulomb | +79 / +30 | 25.3 (25.5) | 0.77 / 0.80 | 73.7 | 103 | 77 | 27 / 17 | 0.00 % |
| MPM Cam Clay (Li II warm shear), Coulomb | -882 / -877 | 30.8 (25.5) | 0.00 / – | 40.6 | 41 | 50 | 18 / 11 | 0.00 % |
| MPM Cam Clay (Li III sliding slab), Coulomb | -878 / -873 | 30.8 (25.5) | 0.00 / – | 39.8 | 40 | 49 | 19 / 10 | 0.00 % |
| MPM Cam Clay (Li IV warm plug), Coulomb | -960 / -955 | 31.2 (25.5) | 0.00 / – | 36.1 | 41 | 57 | 17 / 11 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.40 | +99 / +44 | 25.0 (25.5) | 0.94 / 0.70 | 73.2 | 110 | 68 | 28 / 16 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.47 | +123 / -25 | 25.9 (25.5) | 0.68 / 0.87 | 61.2 | 99 | 70 | 28 / 16 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.55 | +0 / -291 | 28.0 (25.5) | 0.15 / 0.90 | 46.3 | 96 | 70 | 27 / 15 | 0.00 % |

### avaMal (reference avaMalNullTest: 133.3 ha, max pfv 53 m/s, com1DFA run time 174 s)

| model | runout diff. ppr>1 kPa / pft>0.1 m [m] | runout angle [°] (com1DFA) | runout area recall / precision (pft) | footprint [ha] | max pfv [m/s] | t_sim [s] | wall / GPU [s] | walls |
|---|---|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | -119 / -115 | 26.6 (25.4) | 0.28 / 1.00 | 110.6 | 64 | – | 4760 / – | – |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | -27 / -22 | 25.6 (25.4) | 0.84 / 0.36 | 148.8 | 67 | – | 1 / – | – |
| MPM Stomakhin 2013, Coulomb | -749 / -749 | 28.8 (25.4) | 0.00 / – | 121.5 | 53 | 89 | 3 / 2 | 0.00 % |
| MPM Stomakhin 2013, Voellmy | +111 / +115 | 24.3 (25.4) | 0.95 / 0.37 | 185.6 | 80 | 121 | 4 / 2 | 0.00 % |
| MPM Drucker-Prager, Coulomb | ≥+231 / +204 | 23.6 (25.4) | 0.50 / 0.19 | 196.0 | 119 | 91 | 26 / 15 | 0.00 % |
| MPM Drucker-Prager, Voellmy | ≥+231 / ≥+231 | 23.5 (25.4) | 1.00 / 0.13 | 300.8 | 102 | 109 | 31 / 20 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb | -226 / -279 | 27.4 (25.4) | 0.04 / 1.00 | 136.2 | 130 | 93 | 27 / 15 | 0.00 % |
| MPM Cam Clay (Li V), Voellmy | ≥+231 / ≥+231 | 23.5 (25.4) | 1.00 / 0.16 | 260.5 | 119 | 132 | 38 / 23 | 0.00 % |
| MPM Cam Clay (Li I cold dense), Coulomb | +98 / +93 | 24.5 (25.4) | 0.24 / 0.26 | 174.1 | 103 | 91 | 25 / 14 | 0.00 % |
| MPM Cam Clay (Li II warm shear), Coulomb | -676 / -694 | 28.2 (25.4) | 0.00 / – | 126.3 | 54 | 80 | 23 / 14 | 0.00 % |
| MPM Cam Clay (Li III sliding slab), Coulomb | -737 / -732 | 28.5 (25.4) | 0.00 / – | 132.8 | 51 | 79 | 22 / 13 | 0.00 % |
| MPM Cam Clay (Li IV warm plug), Coulomb | -772 / -768 | 29.0 (25.4) | 0.00 / – | 95.4 | 47 | 58 | 13 / 9 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.40 | +173 / +67 | 24.8 (25.4) | 0.67 / 0.35 | 167.9 | 139 | 79 | 21 / 12 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.47 | -132 / -173 | 27.0 (25.4) | 0.12 / 1.00 | 139.9 | 134 | 89 | 22 / 14 | 0.00 % |
| MPM Cam Clay (Li V), Coulomb mu 0.55 | -434 / -462 | 27.7 (25.4) | 0.00 / – | 125.5 | 110 | 93 | 25 / 15 | 0.00 % |

### All five avalanches (pft > 0.1 m)

| model | datasets | mean abs. runout difference [m] | of them at the path end (lower bound) | mean abs. runout angle difference [°] | mean recall / precision | max pfv range [m/s] |
|---|---|---|---|---|---|---|
| com4FlowPy (AvaFrame, defaults) | 5 | 186 | 0 | 2.5 | 0.70 / 0.58 | 58–81 |
| weBIGeo trajectories (alpha 25, theta 24, p 0.4) | 5 | 229 | 1 | 3.4 | 0.94 / 0.33 | 62–86 |
| MPM Stomakhin 2013, Coulomb | 5 | 368 | 0 | 2.2 | 0.36 / 0.86 | 50–60 |
| MPM Stomakhin 2013, Voellmy | 5 | 308 | 3 | 4.3 | 0.84 / 0.33 | 75–88 |
| MPM Drucker-Prager, Coulomb | 5 | 192 | 0 | 2.7 | 0.88 / 0.35 | 101–132 |
| MPM Drucker-Prager, Voellmy | 5 | 326 | 3 | 4.5 | 0.95 / 0.17 | 100–116 |
| MPM Cam Clay (Li V), Coulomb | 5 | 140 | 0 | 1.7 | 0.70 / 0.64 | 97–130 |
| MPM Cam Clay (Li V), Voellmy | 5 | 294 | 3 | 4.0 | 0.94 / 0.19 | 101–127 |
| MPM Cam Clay (Li I cold dense), Coulomb | 5 | 103 | 0 | 1.4 | 0.79 / 0.43 | 98–125 |
| MPM Cam Clay (Li II warm shear), Coulomb | 5 | 355 | 0 | 2.1 | 0.32 / 0.83 | 41–75 |
| MPM Cam Clay (Li III sliding slab), Coulomb | 5 | 380 | 0 | 2.3 | 0.29 / 0.88 | 40–55 |
| MPM Cam Clay (Li IV warm plug), Coulomb | 5 | 551 | 0 | 3.7 | 0.16 / 0.92 | 41–57 |
| MPM Cam Clay (Li V), Coulomb mu 0.40 | 5 | 141 | 0 | 2.1 | 0.87 / 0.39 | 106–139 |
| MPM Cam Clay (Li V), Coulomb mu 0.47 | 5 | 111 | 0 | 1.5 | 0.74 / 0.61 | 99–134 |
| MPM Cam Clay (Li V), Coulomb mu 0.55 | 5 | 175 | 0 | 1.4 | 0.59 / 0.70 | 96–110 |
