# Our model vs. AvaFrame com1DFA (vs. AvaFrame com4FlowPy)

All datasets (DEMs and release areas) are from the [AvaFrame project](https://docs.avaframe.org/en/stable/testing.html).

All tests were run with parameters
 - runout angle $\alpha = 25°$
 - max deviation $\theta = 24°$
 - persistence $p=0.4$
 - step size (in xy-projected distance) $s=2m$
 - num particles per release cells $n = 8$

For `com1DFA` and `com4FlowPy` we use default parameters.

## Synthetic datasets

### `avaBowl`
![avaBowl](images/model-evaluation/avaBowl.png)

### `avaInclinedPlane`
![avaInclinedPlane](images/model-evaluation/avaInclinedPlane.png)

### `avaParabola`
![avaParabola](images/model-evaluation/avaParabola.png)

### `avaSlide`
![avaParabola](images/model-evaluation/avaSlide.png)

### `avaHockeySmall`
![avaHockeySmall](images/model-evaluation/avaHockeySmall.png)

### `avaHockeyChannel`
#### Release scenario 1
![avaHockeyChannel release scenario 1](images/model-evaluation/avaHockeyChannel_release1HS.png)
#### Release scenario 2
![avaHockeyChannel release scenario 2](images/model-evaluation/avaHockeyChannel_release2HS.png)
#### Release scenario 3
![avaHockeyChannel release scenario 3](images/model-evaluation/avaHockeyChannel_release3HS.png)

### `avaHelix`
![avaHelix](images/model-evaluation/avaHelix.png)

### `avaHelixChannel`
![avaHelixChannel](images/model-evaluation/avaHelixChannel.png)

### `avaPyramid`
![avaPyramid](images/model-evaluation/avaPyramid.png)

### `avaPyramid45`
![avaPyramid45](images/model-evaluation/avaPyramid45.png)

## Real-world datasets

### `avaGar`
#### Release scenario 1
![avaGar release scenario 1](images/model-evaluation/avaGar_relGar.png)
#### Release scenario 2
![avaGar release scenario 2](images/model-evaluation/avaGar_relGar2.png)
#### Release scenario 3
![avaGar release scenario 3](images/model-evaluation/avaGar_relGar6.png)

### `avaHit`
![avaHit](images/model-evaluation/avaHit.png)

### `avaKot`
![avaKot](images/model-evaluation/avaKot.png)

### `avaMal`
![avaMal](images/model-evaluation/avaMal.png)

### `avaWog`
![avaWog](images/model-evaluation/avaWog.png)
