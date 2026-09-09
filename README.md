# seiscomp-map-projections

Extra map projections for the SeisComP GUI framework (`libseiscomp_gui`),
built as runtime-loadable plugins.

This repository currently provides one plugin, **`libmapazimuthal`**, with
four spherical **azimuthal ("globe")** projections — an open alternative
to the "Spherical" projection in gempa's commercial `mapprojections`
plugin, and one projection for each fundamental map property:

| `scheme.map.projection` | property | what it is |
|---|---|---|
| `Orthographic` | perspective | The globe seen from infinite distance. One hemisphere, undistorted at the view centre, foreshortened toward the rim. |
| `Stereographic` | conformal | One hemisphere with local shapes/angles preserved; area grows toward the rim. |
| `AzimuthalEquidistant` | equidistant | The whole Earth in a disc; straight-line distance from the view centre is true to scale (teleseismic distance context). |
| `LambertAzimuthalEqualArea` | equal-area | The whole Earth in a disc with areas preserved everywhere. The azimuthal companion to Equal Earth; the basis of EPSG:3035 and the common polar / sea-ice grids. |

Any GUI application (`scmv`, `scolv`, `scrttv`, `scesv`, `scconfig` map
preview, …) can then render map tiles, station coordinates and event
markers with them.

> The Equal Earth projection plugin lives in its own repository for now;
> it may move here later.

## How it works

The projections are **not** rectangular, so the canvas hands `render()` an
`ARGB32` buffer: the map is a disc, pixels outside it — and geographic
points on the far side of the globe — are left fully transparent.

All three share one base class (`AzimuthalProjection`). A point at angular
distance `c` and azimuth `Az` from the view centre `(φ₁, λ₀)` is drawn at
polar radius `ρ(c)` and the same azimuth (Snyder, *Map Projections — A
Working Manual*, USGS PP 1395, 1987, general azimuthal forward/inverse):

```
cos c = sin φ₁ sin φ + cos φ₁ cos φ cos(λ − λ₀)
x = k'·cos φ·sin(λ − λ₀)
y = k'·(cos φ₁ sin φ − sin φ₁ cos φ cos(λ − λ₀))     k' = ρ(c) / sin c
```

Each concrete projection only supplies `ρ(c)`, its inverse, the limb
radius and the maximum drawn angular distance:

| | `ρ(c)` | limb | max `c` |
|---|---|---|---|
| Orthographic | `sin c` | 1 | 90° |
| Stereographic | `2 tan(c/2)` | 2 | 90° |
| AzimuthalEquidistant | `c` | π | 180° |
| LambertAzimuthalEqualArea | `2 sin(c/2)` | 2 | 180° |

`ρ` is normalised by the limb radius so the visible disc always fills the
viewport, then scaled by the shared zoom. `render()` inverse-projects every
pixel inside the disc bounding box; `project()` / `unproject()` return
`false` for the hidden hemisphere and for pixels off the disc. Polygon
outlines (`project(QPainterPath&, …)`) are interpolated along each edge
(great circles curve) and split wherever the line leaves the disc.

## Build

Needs the SeisComP SDK headers/libraries and Qt 5 (`Core`, `Gui`).

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSEISCOMP_ROOT=/opt/seiscomp
cmake --build build
```

Result: `build/libmapazimuthal.so`. `SEISCOMP_ROOT` defaults to
`$SEISCOMP_ROOT` then `/opt/seiscomp`; the build accepts either
`libseiscomp_gui` or `libseiscomp_qt` for the GUI classes.

## Deploy

```bash
install -m 644 build/libmapazimuthal.so           "$SEISCOMP_ROOT/share/plugins/"
install -m 644 etc/descriptions/mapprojections.xml "$SEISCOMP_ROOT/etc/descriptions/"
```

(or `cmake --install build`; `sudo` if `$SEISCOMP_ROOT` is root-owned).
`$SEISCOMP_ROOT/share/plugins` is on the default plugin search path.

## Enable

In `~/.seiscomp/global.cfg` (all GUIs) or a module's `.cfg`:

```
plugins = ${plugins}, libmapazimuthal
scheme.map.projection = Orthographic     # Stereographic / AzimuthalEquidistant / LambertAzimuthalEqualArea
```

Verify via *Help → Loaded Plugins*. Where an application offers a
projection drop-down the new entries appear there too.

## Testing

```bash
cmake -B build -DBUILD_TESTS=ON -DSEISCOMP_ROOT=/opt/seiscomp
cmake --build build
ctest --test-dir build --output-on-failure
```

`tests/regression.cpp` loads the built plugin and, for each projection,
checks the disc geometry (centre, poles, known limb fractions), the
project/unproject round trip, disc clipping, the bounding box, and the
polygon interpolation / limb-splitting behaviour.

## Known limitations

* Polygons straddling the limb are clipped into open arcs rather than
  being closed along the limb circle, so a *filled* feature that crosses
  the horizon may not fill perfectly at the edge. Points, lines, tiles and
  fully-visible polygons are fine.
* `Stereographic` is capped at one hemisphere (90°) to keep the disc
  bounded; the value is a single constant (`cMax()`) if a wider view is
  wanted.
* Zoom-out is clamped to "the globe fills the viewport".

## License

GNU Affero General Public License v3.0 — see [LICENSE](LICENSE). This
matches the license of the SeisComP framework the plugin links against.
