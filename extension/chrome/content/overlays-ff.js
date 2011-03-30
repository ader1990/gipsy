Components.utils.import("resource://gipsy/util.jsm");

var myId    = "gipsy-button"; // ID of button to add
var afterId = "search-container";    // ID of element to insert after
var navBar  = document.getElementById("nav-bar");

if (navBar && !get_bool_pref('btn_toolbar_added')) {
    var curSet  = navBar.currentSet.split(",");
    if (curSet.indexOf(myId) == -1) {
        var pos = curSet.indexOf(afterId) + 1 || curSet.length;
        var set = curSet.slice(0, pos).concat(myId).concat(curSet.slice(pos));

        navBar.setAttribute("currentset", set.join(","));
        navBar.currentSet = set.join(",");
        document.persist(navBar.id, "currentset");
        try {
            BrowserToolboxCustomizeDone(true);
        } catch (e) {}
        set_bool_pref('btn_toolbar_added', true);
    }
}