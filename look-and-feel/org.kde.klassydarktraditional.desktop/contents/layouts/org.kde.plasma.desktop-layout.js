loadTemplate("org.kde.klassy.plasma.desktop.bottomPanel");

const kwinConfig = ConfigFile('kwinrc');
kwinConfig.group = 'Effect-windowview';
kwinConfig.writeEntry('BorderActivateAll', '3'); //present windows all desktops bottom-right
