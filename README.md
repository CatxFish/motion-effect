# motion-filter
Motion-filter is an obs-studio filter plugin for source item animation by updating transform settings.

# Features
- Source animation (linear or bezier curve) and scaling.
- One way (just forward) or Round trip (forward and backward) movement.
- Trigger by hotkey.

# Download
See [Release Page](https://github.com/CatxFish/motion-filter/releases)

# Screenshots
![Linear](https://github.com/CatxFish/motion-filter/blob/master/img/motion.gif)
![Multiple](https://github.com/CatxFish/motion-filter/blob/master/img/motion3.gif)

# Usage
- Add a motion filter to a **scene** (this filter won't work if applied directly to a source). If you want two-way movement, make sure you choose the _Motion-filter (Round trip)_ variant of the filter.
- On the filter property page, choose the source you wish to animate and provide the control points for the animation.
- Use the Forward (and Backward) toggle button to check the results.
- Go to hotkeys page in OBS settings and set hotkey(s) for the motion(s) within the scene.
- That's everything!

# Build
### Windows
First follow build procedures for [obs-studio](https://github.com/obsproject/obs-studio/wiki/install-instructions#windows-build-directions).

- Building obs-studio will produce an `obs.lib` file, generated inside the build directories - e.g. `obs-studio/build/libobs/debug/obs.lib`

- Assuming you have cmake, prior to first configure, add the following entries:

| Entry name         | Type     | Value (e.g.)                |
|--------------------|----------|-----------------------------|
| LIBOBS_LIB         | FILEPATH | /obs-studio/path/to/obs.lib |
| LIBOBS_INCLUDE_DIR | PATH     | /obs-studio/libobs          |

- Click 'Configure', which will run
- Click 'Generate'

This should produce the desired development environment, which after building, shall produce the plugin dll file.

### Linux (Test on Ubuntu)
You have to download obs-studio source code first and make sure you have installed cmake.
```
git clone https://github.com/CatxFish/motion-filter.git
cd motion-filter
mkdir build && cd build
cmake -DLIBOBS_INCLUDE_DIR="<libobs path>" -DCMAKE_INSTALL_PREFIX=/usr ..
make -j4
sudo make install
```
