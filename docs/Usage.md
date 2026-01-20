# Usage

## Running

You can run webigeo either

- in the web, at [webigeo.alpinemaps.org](https://webigeo.alpinemaps.org/) using **a browser that supports the [WebGPU API](https://developer.mozilla.org/en-US/docs/Web/API/WebGPU_API)** (for example Google Chrome), or
- natively, by following the [Setup guide](Setup.md#native).

Once the app is running, a GPS track can be loaded and the avalanche simulation can be run for the region around the track.

![screenshot, full interface](images/interface.jpg)
*Default view after starting the app.*

## Track loading

webigeo supports loading and displaying GPS tracks. A track can be uploaded by clicking `Open GPX File` in the `Track` section of the UI panel. After loading a track, the camera is automatically moved such that the entire track is visible from above.

For now, track loading is the only way of selecting a region for avalanche simulations.

The setting `Line render mode` defines how tracks are displayed. The following options are available:
 - `none`: Tracks are not rendered
 - `without depth test`: Draw tracks, even if they are behind terrain
 - `with depth test`: Draw tracks only if they are not occluded
 - `semi-transparent`: Draw tracks normally if not occluded and semi-transparently if behind terrain

![screenshot, loaded track](images/loaded-track.jpg)
*Viewing the track `examples/Grosse_Kesselspitze.gpx`.*

### Track format

Currently, the only supported track format is [GPX](https://en.wikipedia.org/wiki/GPS_Exchange_Format). GPX is an XML-based format that allows to store a sequence of coordinates with associated properties such as time and elevation. Loaded tracks are displayed by drawing a red line that connects all `<trkpt>` elements in the GPX file ordered by time, regardless of segment or route association. `<wpt>` and `<rte>` elements are ignored.

A list of example GPX files is available in the repository at `example/gpx-tracks`.

## Avalanche simulation

The simulation can be triggered by clicking `Run` in the `Compute pipeline` section of the UI panel. When the settings are changed, a re-run of the simulation triggered. Simulations are always run for the region of the most recently selected track. If no track was selected, the simulation cannot be started.

The `Strength` slider can be used to adjust the opacity of the avalanche results overlay.

![screenshot, avalanche simulation output](images/simulation.jpg)
*Avalanche simulation result for the default settings and the track `example/Schneeberg_Fadensteig_Breite_Ries.gpx`. Overlay opacity is set to 70%.*

### Input data

The input data for the simulation is the [terrain height as raster data](https://en.wikipedia.org/wiki/Digital_elevation_model) within the selected region. One can switch between either using the DSM (Digital Surface Model, includes trees, buildings, etc.) or DTM (Digital Terrain Model, represents just the ground). For avalanche simulation, generally the DTM is preferred. Additionally, the spatial resolution of the height raster can be adjusted using the `Zoom level` slider. For details about zoom level resolutions, see [here](https://wiki.openstreetmap.org/wiki/Zoom_levels).

### Release points

The simulation starts by identifying possible points, where avalanches may start, so called __release points__. Any point with a slope angle between 30 and 60 degrees is considered a release point. This interval may be adjusted in the UI using the sliders for `Release point steepness`. Also, the interval of raster cells that are checked for being possible release points can be set using the slider `Release point interval`. For example, an interval of 4 means, only every fourth cell of the input height raster is checked for being a release cell.

### Trajectories

Now avalanche trajectories are computed. An __avalanche trajectory__ is a connected sequence of positions, starting from a release cell. Each trajectory is extended with new positions iteratively. Each iteration is called a __simulation step__. The maximal number of simulation steps can be set using the slider `Num steps`.

A __routing model__ dictates how the new position is calculated in each step. We experimented with different routing models, but currently only our default model is available to use. This model is a [Monte Carlo method](https://en.wikipedia.org/wiki/Monte_Carlo_method). For every release point, multiple trajectories are computed. In every simulation step, the current direction is randomly offset by a small amount. The final result is the aggregation of all individual trajectories. The parameters are

- `Paths per release point`: How many trajectories to start for each point. 
- `Random contribution`: How large the random offset for each step is. A value of zero means no randomness, and only use the local gradient. A larger value results in wider avalanches (if paths per release point is adaquately large).
- `Persistence`: How much "momentum" (i.e. previous directions) contributes to the direction each step. A larger value means straighter avalanches. A value of zero means to follow the local direction (i.e. local gradient with random offset).

Trajectories are not only terminated when the maximum number of simulation steps is reached, but also by an additional condition called __runout model__. A typical runout model looks at the heights along the path and decides when a typical avalanche should actually stop. This is usually much earlier than when the maximum number of simulation steps is exceeded. The runout model can be set using the `Runout model` combo box. Currently, only a single runout model is supported, the [FlowPy runout model](https://docs.avaframe.org/en/latest/theoryCom4FlowPy.html). This model takes a single parameter, the angle `Alpha` (adjustable via slider). A lower angle angle means longer trajectories. Typical values are between 15 and 35 degrees.

## Snow effect

webigeo also supports rendering snow. This is the same effect used in AlpineMaps. It can be enabled in the `Engine settings` section using the `Snow` checkbox. The snow effect may be combined with the avalanche simulation overlay. 

![screenshot, avalanche simulation output and snow](images/simulation-with-snow.jpg)
*Avalanche simulation results with snow enabled for the track `example/example/Schneeberg_Fadensteig_Breite_Ries.gpx`. Simulation parameters adjusted.*