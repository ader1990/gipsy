#include "xpcom-config.h"
#include "nscore.h"
#include "nsCRT.h"
#include "nsStringAPI.h"
#include "plstr.h"
#include "prmem.h"

#include "nsISupports.h"
#include "nsIPrefService.h"
#include "nsIObserverService.h"
#include "nsISimpleEnumerator.h"
#include "nsEnumeratorUtils.h"
#include "nsProxyRelease.h"
#include "nsIProxyObjectManager.h"
#include "nsServiceManagerUtils.h"
#include "nsXPCOMCIDInternal.h"

#include "prthread.h"

#include "IGPSScanner.h"
#include "gipsy.h"
#include "gpslib/igc.h"
#include "gpslib/foreignigc.h"
#include "tracklog.h"

#include "gipsyversion.h"

#include <string>
#include <utility>
#include <vector>
#include <map>
#include <sstream>

#include <string.h>
#include <time.h>

using namespace std;

#define PREF_PREFIX "extensions.gipsy."

#ifdef WIN32
static void sleep(int time)
{
    _sleep(time*1000);
}
#endif

/* Implementation file */
NS_IMPL_ISUPPORTS1(Gipsy, IGPSScanner);

Gipsy::Gipsy() : exit_thread(false), auto_download(true), observer_count(0)
{
    lock = PR_NewLock();
    prefs_load();
}

/* Destructor code */
Gipsy::~Gipsy()
{
    PR_DestroyLock(lock);
}

struct DownInfo {
    DownInfo(Gipsy *c, PRInt32 p) : cls(c), gpspos(p) {};
    DownInfo(Gipsy *c, IGPSScanner *m, PRInt32 p) : cls(c), gpspos(p), main(m) {};

    Gipsy *cls;
    PRInt32 gpspos;
    IGPSScanner *main;
};

/* Send notification to observer waiting for tracklog */
void Gipsy::notify(nsISupports *subject, const char *topic)
{
    nsresult rv;

    nsCOMPtr<nsIProxyObjectManager> proxyman;
    proxyman = do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);

    nsCOMPtr<nsIObserverService> mainosvc = do_GetService("@mozilla.org/observer-service;1");
    nsCOMPtr<nsIObserverService> observersvc;
 
    rv = proxyman->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD, nsIObserverService::GetIID(),
				     mainosvc, NS_PROXY_ASYNC | NS_PROXY_ALWAYS, getter_AddRefs(observersvc));
    
    if (! NS_FAILED(rv))
	observersvc->NotifyObservers(subject, topic, NULL);
}

/* Set true/false if scan should be enabled/disabled */
bool Gipsy::prefs_scan_enabled(const GpsItem &item)
{
    if (item.portinfo.usb_vendor) {
	if (item.portinfo.usb_vendor == GARMIN_VENDOR)
	    return true;

	for (unsigned int i=0; i < DisabledUSB.size(); i++)
	    if (item.portinfo.usb_vendor == DisabledUSB[i])
		return false;
    } else {
	for (unsigned int i=0; i < DisabledPort.size(); i++)
	    if (item.portinfo.devname == DisabledPort[i])
		return false;
    }
    return true;
}

int Gipsy::prefs_gpstype(const GpsItem &item)
{
    if (item.portinfo.usb_vendor) {
	if (item.portinfo.usb_vendor == GARMIN_VENDOR)
	    return GpsItem::G_GARMIN;

	if (GpsTypesUSB.count(item.portinfo.usb_vendor))
	    return GpsTypesUSB[item.portinfo.usb_vendor];
    } else {
	if (GpsTypesPort.count(item.portinfo.devname))
	    return GpsTypesPort[item.portinfo.devname];
    }
    return GpsItem::G_GARMIN;
}

/* Save preferences (scan device selection) */
void Gipsy::prefs_save(void)
{
    nsresult rv;

    nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1");
    nsCOMPtr<nsIPrefBranch> branch;
    rv = prefs->GetBranch(PREF_PREFIX, getter_AddRefs(branch));
    if (NS_FAILED(rv))
	return;

    // DisabledUSB
    stringstream iprefs;
    
    for (unsigned int i=0; i < DisabledUSB.size(); i++)
	if (DisabledUSB[i] != GARMIN_VENDOR)
	    iprefs << DisabledUSB[i] << ' ';
    iprefs << '\0';

    branch->SetCharPref("usbdisabled", iprefs.str().c_str());

    // DisabledPort
    string sprefs;
    for (unsigned int i=0; i < DisabledPort.size(); i++) 
	sprefs += DisabledPort[i] + '|';
    branch->SetCharPref("portdisabled", sprefs.c_str());

    // GpsTypesUSB
    stringstream gtprefs;
    for (map<unsigned int,int>::iterator iter = GpsTypesUSB.begin();
	 iter != GpsTypesUSB.end(); ++iter) {
	gtprefs << (*iter).first << '|' << (*iter).second << ';';
    }
    gtprefs << '\0';
    branch->SetCharPref("gpsusbtypes", gtprefs.str().c_str());

    // GpsTypesPort
    stringstream sgtprefs;
    for (map<string,int>::iterator iter = GpsTypesPort.begin();
	 iter != GpsTypesPort.end(); ++iter) {
	sgtprefs << (*iter).first << '|' << (*iter).second << ';';
    }
    sgtprefs << '\0';
    branch->SetCharPref("gpsporttypes", sgtprefs.str().c_str());
    
}

/* Load preferences */
void Gipsy::prefs_load(void)
{
    char *result;
    nsresult rv;

    nsCOMPtr<nsIPrefService> prefs = do_GetService("@mozilla.org/preferences-service;1");
    nsCOMPtr<nsIPrefBranch> branch;
    rv = prefs->GetBranch(PREF_PREFIX, getter_AddRefs(branch));

    if (NS_FAILED(rv))
	return;

    rv = branch->GetCharPref("usbdisabled", &result);
    if (!NS_FAILED(rv)) {
	prefs_parse_disusb(result);
	PR_Free(result);
    }

    rv = branch->GetCharPref("portdisabled", &result);
    if (!NS_FAILED(rv)) {
	prefs_parse_disport(result);
	PR_Free(result);
    }

    rv = branch->GetCharPref("gpsusbtypes", &result);
    if (!NS_FAILED(rv)) {
	prefs_parse_usbgpstype(result);
	PR_Free(result);
    }
    rv = branch->GetCharPref("gpsporttypes", &result);
    if (!NS_FAILED(rv)) {
	prefs_parse_portgpstype(result);
	PR_Free(result);
    }
    
    branch->GetBoolPref("autodownload", &auto_download);
}

/* Function running on thread, to access main thread, use main pointer */
/* Thread that scans for new GPSes, removals etc. */
void Gipsy::scanner_thread(IGPSScanner *main)
{
    while (!exit_thread) {
	// Scan for new ports
	PortList plist = get_ports(true);

	vector<unsigned int> changed;
	vector<unsigned int> removed;

	PR_Lock(lock);
	/* Remove non-existent ports */
	for (unsigned int i=0; i < gpslist.size(); i++) {
	    if (! gpslist[i] || gpslist[i]->watcher_running)
		continue;

	    /* Find port */
	    unsigned int j;
	    for (j=0; j < plist.size(); j++)
		if (plist[j].device == gpslist[i]->portinfo.device)
		    break;
	    if (j < plist.size())
		continue;

	    /* Device does not exist anymore, remove it */
	    NS_RELEASE(gpslist[i]);
	    gpslist[i] = NULL;
	    removed.push_back(i);
	}

	/* Find new ports */
	for (unsigned int i=0; i < plist.size(); i++) {
	    int pos;
	    if (find_gps(plist[i].device, pos)) {
		// Start watcher if not already started
		// may happen when scanner is stopped and started again
		if (gpslist[pos]->scan_enabled && !gpslist[pos]->watcher_running)
		    gpslist[pos]->start_watcher();
		continue; // Port already in list
	    }

	    /* Add new */
	    GpsItem *newitem = new GpsItem();
	    NS_ADDREF(newitem);

	    newitem->portinfo = plist[i];
	    // Scan_enable according prefs
	    newitem->scan_enabled = prefs_scan_enabled(*newitem);
	    newitem->gpstype = prefs_gpstype(*newitem);
	    newitem->auto_download = newitem->gpstype == GPS_MLR ? false : auto_download;

	    /* Find new slot */
	    for (pos=0; pos < (int)gpslist.size(); pos++)
		if (!gpslist[pos])
		    break;
	    if (pos < (int)gpslist.size())
		gpslist[pos] = newitem;
	    else
		gpslist.push_back(newitem);

	    newitem->pos = pos;
	    changed.push_back(pos);

	    if (newitem->scan_enabled)
		newitem->start_watcher();
	}
	PR_Unlock(lock);
	// End of critical section

	// Send updates to observers
	for (unsigned int i=0; i < removed.size(); i++) {
	    GpsItem *tmp = new GpsItem();
	    tmp->pos = removed[i];
	    NS_ADDREF(tmp);
	    notify(tmp, "gps_removed");
	    NS_RELEASE(tmp);
	}
	
	for (unsigned int i=0; i < changed.size(); i++) {
	    PRInt32 idx = changed[i];
	    notify(gpslist[idx], "gps_changed");
	}
	sleep(1);
    }
    // Abort watchers on scanner exit
    // TODO: Join them?
    PR_Lock(lock);
    for (unsigned int i=0; i < gpslist.size(); i++)
	if (gpslist[i])
	    gpslist[i]->exit_thread = true;
    PR_Unlock(lock);
}


/* Helper function for scanner thread */
void Gipsy::_scanner_thread(void *arg)
{
    Gipsy *cls = (Gipsy *) arg;
    nsCOMPtr<IGPSScanner> main;
    nsCOMPtr<nsIProxyObjectManager> proxyman;
    nsresult rv;

    proxyman = do_GetService(NS_XPCOMPROXY_CONTRACTID, &rv);
    NS_ASSERTION(gRDFService, "unable to get Proxy Manager service");

    rv = proxyman->GetProxyForObject(NS_PROXY_TO_MAIN_THREAD, IGPSScanner::GetIID(),
				     (IGPSScanner *) cls, NS_PROXY_SYNC | NS_PROXY_ALWAYS, 
				     getter_AddRefs(main));
    if(NS_FAILED(rv)) 
	return;

    cls->scanner_thread(main);
}


/* Get position in gps table & return true if resource is correct */
bool Gipsy::find_gps(const string &gpsdev, PRInt32 &pos)
{
    for (unsigned int i=0; i < gpslist.size(); i++)
	if (gpslist[i] && gpslist[i]->portinfo.device == gpsdev) {
	    pos = i;
	    return true;
	}
    return false;
}

/* Start scanner if not already started */
NS_IMETHODIMP Gipsy::StartScanner()
{
    if (!observer_count) {
	exit_thread = false;
	scanner_tid = PR_CreateThread(PR_USER_THREAD,
				      _scanner_thread,
				      (void *)this,
				      PR_PRIORITY_NORMAL,
				      PR_GLOBAL_THREAD,
				      PR_JOINABLE_THREAD,
				      0);
    }
    observer_count++;

    return NS_OK;
}

/* Stop scanner if nobody is observing */
NS_IMETHODIMP Gipsy::StopScanner()
{
    observer_count--;

    if (!observer_count && scanner_tid) {
	exit_thread = true;
	PR_JoinThread(scanner_tid);
	scanner_tid = 0;
    }
    
    return NS_OK;
}

/* void getGpsArray (out unsigned long count, [array, size_is (count), retval] out long retv); */
NS_IMETHODIMP Gipsy::GetGpsArray(PRUint32 *count, PRInt32 **retv)
{
    PR_Lock(lock);

    *count = 0;
    for (unsigned int i=0; i < gpslist.size(); i++) {
	if (gpslist[i])
	    (*count)++;
    }
    
    *retv = (PRInt32*)nsMemory::Alloc(*count * sizeof(PRInt32));

    int pos = 0;
    for (unsigned int i=0; i < gpslist.size(); i++) {
	if (gpslist[i])
	    (*retv)[pos++] = i;
    }

    PR_Unlock(lock);

    return NS_OK;
}

/* IGPSDevInfo getGpsInfo (in unsigned long pos); */
NS_IMETHODIMP Gipsy::GetGpsInfo(PRUint32 pos, IGPSDevInfo **_retval)
{
    PR_Lock(lock);
    
    if (pos >= gpslist.size() || !gpslist[pos])
	*_retval = NULL;
    else {
	*_retval = gpslist[pos];
	NS_ADDREF(*_retval);
    }
    
    PR_Unlock(lock);

    return NS_OK;
}

/* void gpsToggle (in unsigned long pos, in boolean newstate); */
NS_IMETHODIMP Gipsy::GpsToggle(PRUint32 pos, PRBool newstate)
{
    PR_Lock(lock);

    if (pos >= gpslist.size() || !gpslist[pos]) {
	PR_Unlock(lock);
	return NS_OK;
    }

    GpsItem *gps = gpslist[pos];
    if ((gps->scan_enabled && newstate) || (!gps->scan_enabled && !newstate)) {
	PR_Unlock(lock);
	return NS_OK;
    }

	gps->scan_enabled = newstate ? true : false;
    
    /* Update preferences table */
    
    if (newstate) {
	// Remove from disabled table
	if (gps->portinfo.usb_vendor) {
	    for (vector<unsigned int>::iterator it = DisabledUSB.begin();
		 it < DisabledUSB.end(); ++it) {
		if (*it == gps->portinfo.usb_vendor) {
		    DisabledUSB.erase(it);
		    break;
		}
	    }
	} else {
	    for (vector<string>::iterator it = DisabledPort.begin();
		 it < DisabledPort.end(); ++it) {
		if (*it == gps->portinfo.devname) {
		    DisabledPort.erase(it);
		    break;
		}
	    }
	}
    } else {
	if (gps->portinfo.usb_vendor)
	    DisabledUSB.push_back(gps->portinfo.usb_vendor);
	else
	    DisabledPort.push_back(gps->portinfo.devname);
    }
    prefs_save();

    PR_Unlock(lock);
    /* Notify observers */
    gps->last_error = "";
    notify(gps, "gps_changed");

    return NS_OK;
}

/* void gpsDownload (in unsigned long pos); */
NS_IMETHODIMP Gipsy::GpsDownload(PRUint32 pos)
{
    PR_Lock(lock);
    if (pos >= gpslist.size() || !gpslist[pos] \
	|| gpslist[pos]->wstatus != GpsItem::W_CONNECTED) {

	PR_Unlock(lock);
	return NS_OK;
    }
    gpslist[pos]->download_now = true;
    PR_Unlock(lock);

    return NS_OK;
}

/* Resend again a new tracklog notification */
NS_IMETHODIMP Gipsy::GpsReprocess(PRUint32 pos)
{
    PR_Lock(lock);
    if (pos >= gpslist.size() || !gpslist[pos]) {
	PR_Unlock(lock);
	return NS_OK;
    }
    gpslist[pos]->lock();

    if  (!gpslist[pos]->saved_tlog) {
	gpslist[pos]->unlock();
	PR_Unlock(lock);
	return NS_OK;
    }
    // This does not seem to be thread safe
    nsCOMPtr<IGPSIGC> tlog = gpslist[pos]->saved_tlog;

    gpslist[pos]->unlock();
    PR_Unlock(lock);

    // Don't notify inside lock, we might get deadlock
    notify(tlog, "gps_tracklog");

    return NS_OK;
}

/* void gpsChangeType (in long gtype); */
NS_IMETHODIMP Gipsy::GpsChangeType(PRUint32 pos, PRInt32 gtype)
{
    PR_Lock(lock);

    if (pos >= gpslist.size() || !gpslist[pos] \
	|| gpslist[pos]->portinfo.usb_vendor == GARMIN_VENDOR \
	|| gtype == gpslist[pos]->gpstype) {

	PR_Unlock(lock);
	return NS_OK;
    }
    GpsItem *gps = gpslist[pos];

    gps->gpstype = gtype;
    gps->exit_thread = true;
    gps->auto_download = gps->gpstype == GPS_MLR ? false : auto_download;

    if (gps->portinfo.usb_vendor)
	GpsTypesUSB[gps->portinfo.usb_vendor] = gtype;
    else
	GpsTypesPort[gps->portinfo.devname] = gtype;
    prefs_save();
    
    PR_Unlock(lock);
    
    return NS_OK;
}

/**************************/
NS_IMPL_ISUPPORTS1(GpsItem, IGPSDevInfo);

GpsItem::GpsItem() : last_error(""), scan_enabled(0), gpstype(0), watcher_running(false)
{
    _lock = PR_NewLock();
    reset();
}

GpsItem::~GpsItem()
{
    PR_DestroyLock(_lock);
}

/* Reset as if no GPS was connected */
void GpsItem::reset() 
{
    gpsname = "";
    progress= 0;
    wstatus = W_DISCONNECT;
    download_now = false;
    saved_tlog = NULL;
};

/* readonly attribute string last_error; */
NS_IMETHODIMP GpsItem::GetLast_error(char **aLast_error)
{
    *aLast_error = PL_strdup(last_error.c_str());
    if (!*aLast_error)
	return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

/* readonly attribute string devname; */
NS_IMETHODIMP GpsItem::GetDevname(char **aDevname)
{
    *aDevname = PL_strdup(portinfo.devname.c_str());
    if (!*aDevname)
	return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

/* readonly attribute string gpsname; */
NS_IMETHODIMP GpsItem::GetGpsname(char **aGpsname)
{
    lock();
    *aGpsname = PL_strdup(gpsname.c_str());
    unlock();

    if (!*aGpsname)
	return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

/* readonly attribute boolean scan_enabled; */
NS_IMETHODIMP GpsItem::GetScan_enabled(PRBool *aScan_enabled)
{
    *aScan_enabled = scan_enabled;

    return NS_OK;
}

/* readonly attribute long progress; */
NS_IMETHODIMP GpsItem::GetProgress(PRInt32 *aProgress)
{
    *aProgress = progress;

    return NS_OK;
}

/* readonly attribute long pos; */
NS_IMETHODIMP GpsItem::GetPos(PRInt32 *aPos)
{
    *aPos = pos;

    return NS_OK;
}

/* readonly attribute boolean wstatus; */
NS_IMETHODIMP GpsItem::GetWstatus(PRInt16 *aWstatus)
{
    *aWstatus = wstatus;

    return NS_OK;
}

/* readonly attribute long gpstype; */
NS_IMETHODIMP GpsItem::GetGpstype(PRInt32 *aGpstype)
{
    *aGpstype = gpstype;

    return NS_OK;
}

/* Update progress bar during tracklog transfer */
bool GpsItem::progress_updater(int cnt, int tot)
{
    int newval = 0;
    if (tot)
	newval = cnt*100 / tot;

    if (newval > progress + 5 || (newval > 0 && newval < 5)) {
	progress = newval;
	// Remove error if we have some progress;
	last_error = "";
	Gipsy::notify(this, "gps_changed");
    }

    // Reconfigure GPS information on the first point of tracklog
    // for GPS that send info during download
    if (cnt == 0 && gpsname != gps->gpsname) {
	gpsname = gps->gpsname;
	Gipsy::notify(this, "gps_changed");
    }
    return scan_enabled && !exit_thread;
}

/* Trampoline code to start call progress updater */
bool GpsItem::_progress_updater(void *arg, int cnt, int tot)
{
    GpsItem *cls = (GpsItem *)arg;
    return cls->progress_updater(cnt, tot);
}

/* Trampoline code to start watcher thread */
void GpsItem::_watcher_thread(void *arg) 
{
    GpsItem *cls = (GpsItem *) arg;
    cls->watcher_thread();
}

void GpsItem::download_tracklog()
{
    wstatus = W_DOWNLOADING;
    progress = 0;
    Gipsy::notify(this, "gps_changed");
    
    PointArr parr = gps->download_tracklog(_progress_updater, (void *)this);

    if (gpstype == GPS_MLR)
	wstatus = W_CONNECTED;
    else
	wstatus = W_DOWNCOMPLETE;
    Gipsy::notify(this, "gps_changed");
    
    // If tracklog is not empty...
    if (parr.size()) {
	// Add tracklog to downloaded_tracklog vector
	// AddRef will be done in the notification handler
	Igc *igc = new Igc(parr, gps->gpsname, gps->gpsunitid);
	// Add custom signed fields
	igc->x_params.push_back(pair<string,string>("Xversion", GIPSY_VERSION));
	time_t now = time(NULL);
	char tmptime[31];
	strftime(tmptime, 30, "%Y-%m-%d %H:%M:%S GMT", my_gmtime(&now));
	igc->x_params.push_back(pair<string,string>("XDownloadtime", tmptime));
	
	igc->gen_g_record(); // Generate g-record on the downloaded tracklog
	Tracklog *tlog = new Tracklog(igc);

	// If the tracklog does not have any valid tracks, ignore it
	// TODO: notify somehow user about this situation
	int breakcount;
	tlog->IgcBreakCount(&breakcount);
	if (!breakcount) {
	    delete tlog;
	    return;
	}

	// Save to the gps table
	NS_ADDREF(tlog);
	
	lock(); // This does not seem to be thread safe
	saved_tlog = dont_AddRef((IGPSIGC *) tlog);
	unlock();

	// Notify UI of new downloaded tracklog
	Gipsy::notify(tlog, "gps_tracklog");
    }
}
#include <iostream>
/* Thread that communicates with a GPS */
void GpsItem::watcher_thread()
{
    while (scan_enabled && !exit_thread) {
	sleep(1);
	try {
	    gps = make_gps(portinfo.device, gpstype);
	    if (!gps)
		break;	
	} catch (OpenException e) {
	    last_error = e.error;
	    Gipsy::notify(this, "gps_changed");
	    break;
	} catch (Exception e) {
	    last_error = e.error;
	    reset();
	    Gipsy::notify(this, "gps_changed");
	    sleep(1);
	    continue;
	}
	if (gpsname != gps->gpsname) {
	    lock(); // Are these operations atomic?
	    gpsname = gps->gpsname;
	    unlock();

	    Gipsy::notify(this, "gps_changed");
	}
	if (wstatus == W_DISCONNECT || gpstype == G_AIRCOTEC) {
	    wstatus = W_CONNECTED;
	    Gipsy::notify(this, "gps_changed");
	}
	if ((auto_download || download_now) && wstatus == W_CONNECTED) {
	    try {
		download_tracklog();
		last_error = "";
	    } catch (NoData e) {
		// No data - aircotec
		delete gps;
		gps = NULL;
		continue;
	    } catch (Exception e) {
		last_error = e.error;
		Gipsy::notify(this, "gps_changed");
		delete gps;
		gps = NULL;
		break;
	    }
	    download_now = false;
	}
	delete gps; gps = NULL;
    }
    reset();
    Gipsy::notify(this, "gps_changed");
    watcher_running = false;
}

/* Start watching thread */
void GpsItem::start_watcher()
{
    watcher_running = true;
    exit_thread = false;

    PR_CreateThread(PR_USER_THREAD,
		    _watcher_thread,
		    (void *)this,
		    PR_PRIORITY_NORMAL,
		    PR_GLOBAL_THREAD,
		    PR_UNJOINABLE_THREAD,
		    0);

}