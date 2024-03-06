const panel = new Panel;
var panelScreen = panel.screen;
panel.location = "bottom";
panel.height = Math.round(gridUnit * 20 / 9); //gridUnit is 18
panel.hiding = "normal";
panel.offset = 0;

// Restrict horizontal panel to a maximum size of a 21:9 monitor
const maximumAspectRatio = 21/9;
if (panel.formFactor === "horizontal") {
    const geo = screenGeometry(panelScreen);
    const maximumWidth = Math.ceil(geo.height * maximumAspectRatio);

    if (geo.width > maximumWidth) {
        panel.alignment = "center";
        panel.minimumLength = maximumWidth;
        panel.maximumLength = maximumWidth;
    }
}

panel.addWidget("org.kde.plasma.kickoff");
panel.addWidget("org.kde.plasma.pager");
var taskmanager = panel.addWidget("org.kde.plasma.taskmanager");
taskmanager.currentConfigGroup = ["General"];
taskmanager.writeConfig('launchers', 'preferred://filemanager,preferred://browser');
taskmanager.reloadConfig(); //launchers don't get set unless reloadConfig() is called between writes
taskmanager.writeConfig('separateLaunchers', false);
taskmanager.reloadConfig();
panel.addWidget("org.kde.plasma.systemtray");
var digitalclock = panel.addWidget("org.kde.plasma.digitalclock");
digitalclock.currentConfigGroup = ["Appearance"];
digitalclock.writeConfig("customDateFormat", "ddd-d");
digitalclock.writeConfig("dateFormat", "custom");
panel.addWidget("org.kde.plasma.showdesktop");
