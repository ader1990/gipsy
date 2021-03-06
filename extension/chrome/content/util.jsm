var EXPORTED_SYMBOLS = ["sprintf", "chr", "ord", 
                        "get_bool_pref", "set_bool_pref", 
                        "get_string_pref", "set_string_pref", 
                        "empty",
                        "format_km", "format_km0", "format_km2", "format_kmh",
                        "format_m", "format_ms",
                        "findPosX", "findPosY",
                        "sinh",
                        "ctxFillText", "track_color",
			"cvt"
                       ];

/*
**  sprintf.js -- POSIX sprintf(3) style formatting function for JavaScript
**  Copyright (c) 2006-2007 Ralf S. Engelschall <rse@engelschall.com>
**  Partly based on Public Domain code by Jan Moesen <http://jan.moesen.nu/>
**  Licensed under GPL <http://www.gnu.org/licenses/gpl.txt>
**
*/

/*  the sprintf() function  */
function sprintf() {
    /*  argument sanity checking  */
    if (!arguments || arguments.length < 1)
        alert("sprintf:ERROR: not enough arguments");

    /*  initialize processing queue  */
    var argumentnum = 0;
    var done = "", todo = arguments[argumentnum++];

    /*  parse still to be done format string  */
    var m;
    while ((m = /^([^%]*)%(\d+$)?([#0 +'-]+)?(\*|\d+)?(\.\*|\.\d+)?([%diouxXfFcs])(.*)$/.exec(todo))) {
        var pProlog    = m[1],
            pAccess    = m[2],
            pFlags     = m[3],
            pMinLength = m[4],
            pPrecision = m[5],
            pType      = m[6],
            pEpilog    = m[7];

        /*  determine substitution  */
        var subst;
        if (pType == '%')
            /*  special case: escaped percent character  */
            subst = '%';
        else {
            /*  parse padding and justify aspects of flags  */
            var padWith = ' ';
            var justifyRight = true;
            if (pFlags) {
                if (pFlags.indexOf('0') >= 0)
                    padWith = '0';
                if (pFlags.indexOf('-') >= 0) {
                    padWith = ' ';
                    justifyRight = false;
                }
            }
            else
                pFlags = "";

            /*  determine minimum length  */
            var minLength = -1;
            if (pMinLength) {
                if (pMinLength == "*") {
                    var access = argumentnum++;
                    if (access >= arguments.length)
                        alert("sprintf:ERROR: not enough arguments");
                    minLength = arguments[access];
                }
                else
                    minLength = parseInt(pMinLength, 10);
            }

            /*  determine precision  */
            var precision = -1;
            if (pPrecision) {
                if (pPrecision == ".*") {
                    var access = argumentnum++;
                    if (access >= arguments.length)
                        alert("sprintf:ERROR: not enough arguments");
                    precision = arguments[access];
                }
                else
                    precision = parseInt(pPrecision.substring(1), 10);
            }

            /*  determine how to fetch argument  */
            var access = argumentnum++;
            if (pAccess)
                access = parseInt(pAccess.substring(0, pAccess.length - 1), 10);
            if (access >= arguments.length)
                alert("sprintf:ERROR: not enough arguments");

            /*  dispatch into expansions according to type  */
            var prefix = "";
            switch (pType) {
                case 'd':
                case 'i':
                    subst = arguments[access];
                    if (typeof subst != "number")
                        subst = 0;
                    subst = subst.toString(10);
                    if (pFlags.indexOf('#') >= 0 && subst >= 0)
                        subst = "+" + subst;
                    if (pFlags.indexOf(' ') >= 0 && subst >= 0)
                        subst = " " + subst;
                    break;
                case 'o':
                    subst = arguments[access];
                    if (typeof subst != "number")
                        subst = 0;
                    subst = subst.toString(8);
                    break;
                case 'u':
                    subst = arguments[access];
                    if (typeof subst != "number")
                        subst = 0;
                    subst = Math.abs(subst);
                    subst = subst.toString(10);
                    break;
                case 'x':
                    subst = arguments[access];
                    if (typeof subst != "number")
                        subst = 0;
                    subst = subst.toString(16).toLowerCase();
                    if (pFlags.indexOf('#') >= 0)
                        prefix = "0x";
                    break;
                case 'X':
                    subst = arguments[access];
                    if (typeof subst != "number")
                        subst = 0;
                    subst = subst.toString(16).toUpperCase();
                    if (pFlags.indexOf('#') >= 0)
                        prefix = "0X";
                    break;
                case 'f':
                case 'F':
                    subst = arguments[access];
                    if (typeof subst != "number")
                        subst = 0.0;
                    subst = 0.0 + subst;
                    if (precision > -1) {
                        if (subst.toFixed)
                            subst = subst.toFixed(precision);
                        else {
                            subst = (Math.round(subst * Math.pow(10, precision)) / Math.pow(10, precision));
                            subst += "0000000000";
                            subst = subst.substr(0, subst.indexOf(".")+precision+1);
                        }
                    }
                    subst = '' + subst;
                    if (pFlags.indexOf("'") >= 0) {
                        var k = 0;
                        for (var i = (subst.length - 1) - 3; i >= 0; i -= 3) {
                            subst = subst.substring(0, i) + (k == 0 ? "." : ",") + subst.substring(i);
                            k = (k + 1) % 2;
                        }
                    }
                    break;
                case 'c':
                    subst = arguments[access];
                    if (typeof subst != "number")
                        subst = 0;
                    subst = String.fromCharCode(subst);
                    break;
                case 's':
                    subst = arguments[access];
                    if (precision > -1)
                        subst = subst.substr(0, precision);
                    if (typeof subst != "string")
                        subst = "";
                    break;
            }

            /*  apply optional padding  */
            var padding = minLength - subst.toString().length - prefix.toString().length;
            if (padding > 0) {
                var arrTmp = new Array(padding + 1);
                if (justifyRight)
                    subst = arrTmp.join(padWith) + subst;
                else
                    subst = subst + arrTmp.join(padWith);
            }

            /*  add optional prefix  */
            subst = prefix + subst;
        }

        /*  update the processing queue  */
        done = done + pProlog + subst;
        todo = pEpilog;
    }
    return (done + todo);
}

function chr(c) {
      var h = c . toString (16);
      h = unescape ('%'+h);
      return h;
} 

function ord(c) {
      return c.charCodeAt(0);
}

function get_bool_pref(pref) {
	var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
	prefs = prefs.getBranch("extensions.gipsy.");	
	return prefs.getBoolPref(pref);
}

function set_bool_pref(pref, value) {
        var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
        prefs = prefs.getBranch("extensions.gipsy.");   
        return prefs.setBoolPref(pref, value);
}


var cvt = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
cvt.charset = 'utf8';

function get_string_pref(pref) {
	var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
	prefs = prefs.getBranch("extensions.gipsy.");	
	return cvt.ConvertToUnicode(prefs.getCharPref(pref));
}

function set_string_pref(pref, value) {
	var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
	prefs = prefs.getBranch("extensions.gipsy.");	
	return prefs.setCharPref(pref, cvt.ConvertFromUnicode(value));
}

// Remove all children form na element
function empty(el) {
    while ( el.childNodes.length >= 1 )
        el.removeChild(el.firstChild);
}

function format_km(dist) {
    if (get_bool_pref('metric'))
	return sprintf("%.1f km", dist / 1000);
    return sprintf("%.1f mi.", dist / 1609.344);
}

function format_km2(dist) {
    if (get_bool_pref('metric'))
        return sprintf("%.2f km", dist / 1000);
    return sprintf("%.2f mi.", dist / 1609.344);
}

function format_km0(dist) {
    if (get_bool_pref('metric')) {
        if (dist < 1000)
            return sprintf('%d m', dist);
        return sprintf("%d km", dist / 1000);
    }
    return sprintf("%.1f mi.", dist / 1609.344);
}

function format_m(dist) {
    if (get_bool_pref('metric'))
	return sprintf("%d m", Math.round(dist));
    return sprintf("%d ft", Math.round(dist * 3.2808399));
}

function format_kmh(speed) {
    if (get_bool_pref('metric'))
        return sprintf("%.1f km/h", speed * 3.6);
    return sprintf("%.1f mph", speed * 3.6 / 1.609344);
}

function format_ms(speed) {
    if (get_bool_pref('metric'))
	return sprintf("%.1f m/s", speed);
    return sprintf("%d ft/m", Math.round(speed * 3.2808399 * 60));
}    

function findPosX(startobj)
{
    var curleft = 0;
    var obj = startobj;
    while(1)  {
        curleft += obj.offsetLeft;
        if(!obj.offsetParent)
            break;
        obj = obj.offsetParent;
    }
    // Go through all objects anyway and add scrolls
    obj = startobj;
    while (obj) {
        if (obj.scrollLeft)
            curleft -= obj.scrollLeft;
        obj = obj.parentNode;
    }
    
    return curleft;
}

function findPosY(startobj)
{
    var curtop = 0;
    var obj = startobj;
    while(1) {
        curtop += obj.offsetTop;
        if(!obj.offsetParent)
        break;
        obj = obj.offsetParent;
    }
    // Go through all objects anyway and add scrolls
    obj = startobj;
    while (obj) {
        if (obj.scrollTop)
            curleft -= obj.scrollTop;
        obj = obj.parentNode;
    }
    return curtop;
}

function sinh(arg) 
{
    return (Math.exp(arg) - Math.exp(-arg))/2;
}

// Call fill text or older version (FF3.0) if that fails (needed for firefox 3.0)
function ctxFillText(ctx, text, x, y) 
{
    try {
        ctx.fillText(text, x, y);
    } catch (e) {
        ctx.save();
        ctx.mozTextStyle = ctx.font;
        ctx.translate(x, y);
        if (ctx.textBaseline == 'top')
            ctx.translate(0, 8);
        ctx.mozDrawText(text);
        ctx.restore();
    }
}

// Return color of track based on index of track
function track_color(i, op) {
    var r = 255 - ((i * 50) % 128);
    var g = 120 + ((50 + i * 40) % 120);
    var b = (60 + i * 30) % 250;
    if (op)
        return sprintf('rgba(%d,%d,%d,%f)', r, g, b, op);
    else
        return sprintf('rgb(%d,%d,%d)', r, g, b);
}
