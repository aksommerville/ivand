#include "drmfb_internal.h"

/* Connector and mode chosen.
 */
 
static int drmfb_init_with_conn(
  struct drmfb *drmfb,
  drmModeResPtr res,
  drmModeConnectorPtr conn
) {

  /* TODO would be really cool if we could make this work with a "disconnected" monitor, with X running.
   * Understandably, a monitor that X is using will just not be available, I've made peace with that.
   * If I disable the larger monitor via Mate monitors control panel, we still detect it here and try to connect.
   * Its encoder_id is zero, but (conn->encoders) does list the right one.
   * No matter what I do though, when we reach the first drmModeSetCrtc it fails.
   * I'm guessing we have to tell DRM to use this encoder with this connection, but no idea how...
   */

  drmfb->connid=conn->connector_id;
  drmfb->encid=conn->encoder_id;
  drmfb->screenw=drmfb->mode.hdisplay;
  drmfb->screenh=drmfb->mode.vdisplay;
  
  drmModeEncoderPtr encoder=drmModeGetEncoder(drmfb->fd,drmfb->encid);
  if (!encoder) return -1;
  drmfb->crtcid=encoder->crtc_id;
  drmModeFreeEncoder(encoder);
  
  // Store the current CRTC so we can restore it at quit.
  if (!(drmfb->crtc_restore=drmModeGetCrtc(drmfb->fd,drmfb->crtcid))) return -1;

  return 0;
}

/* Filter and compare modes.
 */
 
static int drmfb_connector_acceptable(
  struct drmfb *drmfb,
  drmModeConnectorPtr conn
) {
  if (conn->connection!=DRM_MODE_CONNECTED) return 0;
  // Could enforce size limits against (mmWidth,mmHeight), probably not.
  return 1;
}

static int drmfb_mode_acceptable(
  struct drmfb *drmfb,
  drmModeConnectorPtr conn,
  drmModeModeInfoPtr mode
) {
  //XXX temporary hack to prevent 30 Hz output from the Atari
  if (mode->vrefresh<50) return 0;
  //if (mode->hdisplay<3000) return 0;
  return 1;
}

// <0=a, >0=b, 0=whatever.
// All connections and modes provided here have been deemed acceptable.
// (conna) and (connb) will often be the same object.
static int drmfb_mode_compare(
  struct drmfb *drmfb,
  drmModeConnectorPtr conna,drmModeModeInfoPtr modea,
  drmModeConnectorPtr connb,drmModeModeInfoPtr modeb
) {
  
  // Firstly, if just one has the "PREFERRED" flag, it's in play right now. Prefer that, so we don't switch resolution.
  // Two modes with this flag, eg two monitors attached, they cancel out and we look at other things.
  //TODO I guess there should be an option to override this. As is, it should always trigger.
  int prefera=(modea->type&DRM_MODE_TYPE_PREFERRED);
  int preferb=(modeb->type&DRM_MODE_TYPE_PREFERRED);
  if (prefera&&!preferb) return -1;
  if (!prefera&&preferb) return 1;
  
  // Our framebuffer is surely smaller than any available mode, so prefer the smaller mode.
  if ((modea->hdisplay!=modeb->hdisplay)||(modea->vdisplay!=modeb->vdisplay)) {
    int areaa=modea->hdisplay*modea->vdisplay;
    int areab=modeb->hdisplay*modeb->vdisplay;
    if (areaa<areab) return -1;
    if (areaa>areab) return 1;
  }
  // Hmm ok, closest to 60 Hz?
  if (modea->vrefresh!=modeb->vrefresh) {
    int da=modea->vrefresh-60; if (da<0) da=-da;
    int db=modeb->vrefresh-60; if (db<0) db=-db;
    int dd=da-db;
    if (dd<0) return -1;
    if (dd>0) return 1;
  }

  // If we get this far, the modes are actually identical, so whatever.
  return 0;
}

/* Choose the best connector and mode.
 * The returned connector is STRONG.
 * Mode selection is copied over (G.mode).
 */
 
static drmModeConnectorPtr drmfb_choose_connector(
  struct drmfb *drmfb,
  drmModeResPtr res
) {

  //XXX? Maybe just while developing...
  int log_all_modes=1;
  int log_selected_mode=1;

  if (log_all_modes) {
    fprintf(stderr,"%s:%d: %s...\n",__FILE__,__LINE__,__func__);
  }
  
  drmModeConnectorPtr best=0; // if populated, (drmfb->mode) is also populated
  int conni=0;
  for (;conni<res->count_connectors;conni++) {
    drmModeConnectorPtr connq=drmModeGetConnector(drmfb->fd,res->connectors[conni]);
    if (!connq) continue;
    
    if (log_all_modes) fprintf(stderr,
      "  CONNECTOR %d %s %dx%dmm type=%08x typeid=%08x\n",
      connq->connector_id,
      (connq->connection==DRM_MODE_CONNECTED)?"connected":"disconnected",
      connq->mmWidth,connq->mmHeight,
      connq->connector_type,
      connq->connector_type_id
    );
    
    if (!drmfb_connector_acceptable(drmfb,connq)) {
      drmModeFreeConnector(connq);
      continue;
    }
    
    drmModeModeInfoPtr modeq=connq->modes;
    int modei=connq->count_modes;
    for (;modei-->0;modeq++) {
    
      if (log_all_modes) fprintf(stderr,
        "    MODE %dx%d @ %d Hz, flags=%08x, type=%08x\n",
        modeq->hdisplay,modeq->vdisplay,
        modeq->vrefresh,
        modeq->flags,modeq->type
      );
    
      if (!drmfb_mode_acceptable(drmfb,connq,modeq)) continue;
      
      if (!best) {
        best=connq;
        drmfb->mode=*modeq;
        continue;
      }
      
      if (drmfb_mode_compare(drmfb,best,&drmfb->mode,connq,modeq)>0) {
        best=connq;
        drmfb->mode=*modeq;
        continue;
      }
    }
    
    if (best!=connq) {
      drmModeFreeConnector(connq);
    }
  }
  if (best&&log_selected_mode) {
    fprintf(stderr,
      "drm: selected mode %dx%d @ %d Hz on connector %d\n",
      drmfb->mode.hdisplay,drmfb->mode.vdisplay,
      drmfb->mode.vrefresh,
      best->connector_id
    );
  }
  return best;
}

/* File opened and resources acquired.
 */
 
static int drmfb_init_with_res(
  struct drmfb *drmfb,
  drmModeResPtr res
) {

  drmModeConnectorPtr conn=drmfb_choose_connector(drmfb,res);
  if (!conn) {
    fprintf(stderr,"drm: Failed to locate a suitable connector.\n");
    return -1;
  }
  
  int err=drmfb_init_with_conn(drmfb,res,conn);
  drmModeFreeConnector(conn);
  if (err<0) return -1;

  return 0;
}

/* Initialize connection.
 */
 
int drmfb_init_connection(struct drmfb *drmfb) {

  const char *path="/dev/dri/card0";//TODO Let client supply this, or scan /dev/dri/
  
  if ((drmfb->fd=open(path,O_RDWR))<0) {
    fprintf(stderr,"%s: Failed to open DRM device: %m\n",path);
    return -1;
  }
  
  drmModeResPtr res=drmModeGetResources(drmfb->fd);
  if (!res) {
    fprintf(stderr,"%s:drmModeGetResources: %m\n",path);
    return -1;
  }
  
  int err=drmfb_init_with_res(drmfb,res);
  drmModeFreeResources(res);
  if (err<0) return -1;
  
  return 0;
}
