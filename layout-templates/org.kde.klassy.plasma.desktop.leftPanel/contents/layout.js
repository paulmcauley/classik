const panel = new Panel;
panel.location = "left";
panel.height = Math.round(gridUnit * 10 / 3); //gridUnit is 18
panel.hiding = "normal";
panel.offset = 0;

panel.addWidget("org.kde.plasma.kickoff");
panel.addWidget("org.kde.plasma.pager");
var taskmanager = panel.addWidget("org.kde.plasma.icontasks");
taskmanager.currentConfigGroup = ["General"];
taskmanager.writeConfig('launchers', 'preferred://filemanager,preferred://browser');
taskmanager.reloadConfig();
panel.addWidget("org.kde.plasma.systemtray");
var digitalclock = panel.addWidget("org.kde.plasma.digitalclock");
digitalclock.currentConfigGroup = ["Appearance"];
digitalclock.writeConfig("customDateFormat", "ddd-d");
digitalclock.writeConfig("dateFormat", "custom");
panel.addWidget("org.kde.plasma.showdesktop");

const kwinConfig = ConfigFile('kwinrc');
kwinConfig.group = 'Effect-windowview';
kwinConfig.writeEntry('BorderActivateAll', '5'); //present windows all desktops bottom-left
