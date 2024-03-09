# Klassy

##### Table of Contents
[Overview](#overview)  
[Installation](#installation)  
>[Pre-built packages](#prebuilt)  
>[Compile from source](#compile)  

[System icon theme](#icons)  
[Klassy Settings](#klassy-settings)  
[Donations](#donations)

&nbsp;

<a name="overview"/>

## Overview
_Klassy_ (formerly _ClassiK_/_ClassikStyles_) is a highly customizable binary Window Decoration, Application Style and Global Theme plugin for recent versions of the KDE Plasma desktop. Initially taking inspiration from the iconography of KDE 1, the _Klassy_ defaults are an attempt to create a usable and appealing look for the modern Plasma desktop. Install with the instructions below, and then make sure it is enabled in System Settings -> Appearance -> Window Decorations, in System Settings -> Appearance -> Application Style and in System Settings -> Appearance -> Icons.

![Screenshot of Button icons menu](screenshots/button_icon_menu.png?raw=true "Screenshot of Button icons menu")
>(Above Preset: Glassy Klassy, Colours: Klassy Dark)
* Also provides _Oxygen/Breeze_ and _Redmond_ button icons.
* Provides a Window Decoration, Application Style and Icons so that icons in dockable panels, MDI applications, context menus and GTK applications consistently match the main titlebar icons (make sure to enable all 3 in your Plasma Appearance settings!):
> [![Watch the Klassy demo video](screenshots/video_dummy.png "Watch the Klassy demo video")](http://paulmcauley.com/kde/klassy/klassy_demo.webm)

* Configurable button size, shape, translucency, colours and outlining.

* Default buttons (_Klassy_ preset) are "Integrated Rounded Rectangles" with translucent outlined accent colours, with a large clickable area. These are designed to complement the "Blue Ocean" design refresh in Plasma 5.23, along with the Plasma 5.23 accent colour feature:
> ![Default Integrated Rounded rectangle button style, dark](screenshots/highlight_gifs/icdark.gif?raw=true "Default Integrated Rounded rectangle button style, dark")
* Alternative "Traffic lights" background colours:
> ![Traffic lights button style, dark](screenshots/highlight_gifs/icdark_trafficlights.gif?raw=true "Traffic lights button style, dark")

* Alternative "Full-height Rounded Rectangle" button shape (_ClassiK v3_ preset):
> ![Rounded rectangle button style, dark](screenshots/highlight_gifs/cdark.gif?raw=true "Default button style, dark")![Rounded Rectangle button style, light](screenshots/highlight_gifs/clight.gif?raw=true "Default button style, light")
> ![Pink button style, dark](screenshots/highlight_gifs/pdark.gif?raw=true "Green button style, dark")![Pink button style, light](screenshots/highlight_gifs/plight.gif?raw=true "Pink button style, light")
> ![Turquoise button style, dark](screenshots/highlight_gifs/tdark.gif?raw=true "Turquoise button style, dark")![Turquoise button style, light](screenshots/highlight_gifs/tlight.gif?raw=true "Turquoise button style, light")


* Full-height Rectangular (_ClassikStyles_ preset) or Circular (_Classik-Aurorae_ preset) button highlights:
> ![Full-height Rectangle button highlight style, inheriting system highlight colours](screenshots/highlight_gifs/squareHighlight.gif?raw=true "Full-height Rectangle button highlight style, inheriting system highlight colours")![Circle button highlight style, inheriting system highlight colours](screenshots/highlight_gifs/circleHighlight.gif?raw=true "Circle button highlight style, inheriting system highlight colours")

* Ability to inherit system colour-scheme highlight colours for hover and focus, as well as titlebar colour. Auto-enhances contrast in cases where it is poor.
* Option to have matching titlebar colour and border colour.

* Configurable button spacing (left and right), titlebar margins (sides, top/bottom and option for different maximized), corner radius; all with scaling for HiDPI:
> ![Screenshot of Alignment & Spacing tab](screenshots/alignment_and_spacing.png?raw=true "Screenshot of Alignment & Spacing tab")

* Transparency/opacity configurable for both active/inactive titlebars/headers; setting to make maximized windows opaque; blur is configurable:

* Configurable thin window outline with optional accent colours:
> ![Composite screenshot of thin window outlines with Contrast blended with accent colour (dark)](screenshots/thin_window_outline_dark_contrast_accent.png?raw=true "Composite screenshot of thin window outlines with Contrast blended with accent colour (dark)")
> ![Composite screenshot of thin window outlines with Contrast blended with accent colour (light)](screenshots/thin_window_outline_light_contrast_accent.png?raw=true "Composite screenshot of thin window outlines with Contrast blended with accent colour (light)")

* Configurable scrollbars:
> <img src="screenshots/configurable_scrollbars.png" width="80%">![Default scrollbar animation](screenshots/scrollbar_mouseover.gif?raw=true "Default scrollbar animation")

* Configurable animations

* Ability to load icons from the system icon theme. `window-*-symbolic` icons are recommended for this purpose. If the icons have been designed properly with SVG CSS properties then they will be colourized by default. You can also force the colourization of any symbolic icon by checking the _Force-colourize icons_ window decoration setting.

&nbsp;

<a name="installation"/>

## Installation
_Klassy_ version 5.0 requires at least __Plasma 5.27__, so please check you have this in kinfocenter before trying to install. You can either download the pre-built packages or compile from source.
<a name="prebuilt"/>

### Pre-built packages
[Respositories are available from the Open Build Service](https://software.opensuse.org//download.html?project=home%3Apaul4us&package=klassy) for OpenSUSE, Debian/Ubuntu/KDE Neon, Fedora, Mageia and Arch/Manjaro to keep you on the latest version.

&nbsp;

Raw .rpm, .deb etc. binary packages are also available from the Open Build Service link above if you choose "Grab binary packages directly".

&nbsp;
&nbsp;
<a name="compile"/>

### Compile from source
### Step 1: First, Install Dependencies
#### OpenSUSE Tumbleweed/Leap build dependencies
```
sudo zypper in cmake extra-cmake-modules libQt5Core-devel libQt5Gui-devel libQt5DBus-devel libqt5-qtx11extras-devel libkdecoration2-devel kcoreaddons-devel kguiaddons-devel kconfigwidgets-devel kwindowsystem-devel ki18n-devel kiconthemes-devel kpackage-devel libQt5QuickControls2-devel frameworkintegration-devel kcmutils-devel kirigami2-devel libqt5-qtsvg-devel libQt5Xml-devel
```

#### Debian/Ubuntu/KDE Neon build dependencies
```
sudo apt install build-essential libkf5config-dev libkdecorations2-dev libqt5x11extras5-dev qtdeclarative5-dev extra-cmake-modules libkf5guiaddons-dev libkf5configwidgets-dev libkf5windowsystem-dev libkf5coreaddons-dev gettext cmake libkf5iconthemes-dev libkf5package-dev libkf5style-dev libkf5kcmutils-dev kirigami2-dev libqt5svg5-dev
```

#### Arch/Manjaro build dependencies
```
sudo pacman -S kdecoration qt5-declarative qt5-x11extras cmake make gcc extra-cmake-modules
```

#### Fedora build dependencies
```
sudo dnf install cmake extra-cmake-modules
sudo dnf install "cmake(Qt5Core)" "cmake(Qt5Gui)" "cmake(Qt5DBus)" "cmake(Qt5X11Extras)" "cmake(KF5GuiAddons)" "cmake(KF5WindowSystem)" "cmake(KF5I18n)" "cmake(KDecoration2)" "cmake(KF5CoreAddons)" "cmake(KF5ConfigWidgets)" "cmake(KF5IconThemes)" "cmake(KF5Package)" "cmake(Qt5Quick)" "cmake(KF5FrameworkIntegration)" "cmake(KF5KCMUtils)" "cmake(KF5Kirigami2)" "cmake(Qt5Svg)"
```

### Step 2: Then, build and install
Build and install from source script:
```
./install.sh
```

Uninstall build script:
```
./uninstall.sh
```

&nbsp;
<a name="icons"/>

## System icon theme
To make your Plasma Desktop fully consistent, Klassy auto-generates "Klassy" and "Klassy Dark" system icon themes whenever a window-decoration setting changes. These add consistent titlebar button icons to certain minor context menus in Plasma, and also add consistent GTK application titlebar icons (for Firefox to have consistent titlebar icons on Wayland, it is recommended to set the system GNOME/GTK Application style to Adwaita as Breeze is buggy).
> <img src="screenshots/klassy_square_icons.png" alt="Screenshot of Klassy with icon theme" width="512">

These icons inherit the _Breeze_ icon theme by default, only overriding the titlebar button icons. Another icon theme may be inherited by changing the setting in the Window Decoration settings under the _System Icon Generation..._ button.

<a name="klassy-settings"/>

## Klassy Settings
Klassy includes a settings application, `klassy-settings`. This shows Window Decoration and Application Style settings in one place.

`klassy-settings` also has command-line options to allow Preset file imports, load Presets and generate system icons. Run `klassy-setttings --help` for details.

<a name="donations"/>

## Donations
[![Donate using Liberapay](https://liberapay.com/assets/widgets/donate.svg "Donate using Liberapay")](https://liberapay.com/paulmcauley/donate)
or [![Donate using Paypal](https://www.paypalobjects.com/webstatic/en_US/i/buttons/PP_logo_h_100x26.png "Donate using Paypal")](https://www.paypal.com/donate?business=6N9RP4LDLNZCC&currency_code=GBP)
