#ifndef _FLYMASTER_H_
#define _FLYMASTER_H_

#ifdef WIN32
# include <win_stdint.h>
#else
# include <stdint.h>
#endif

#include "gps.h"
#include "phys.h"

#ifdef WIN32
# define gcc_pack
#pragma pack(push, 1)
#else
# define gcc_pack __attribute__((packed))
#endif

extern "C" {
    
typedef struct {
    uint16_t sw_version;
    uint16_t hw_version;
    uint32_t serial;
    char compnum[8];
    char pilotname[15];
    char gliderbrand[15];
    char glidermodel[15];
} gcc_pack FM_Flight_Info;

typedef struct {
} gcc_pack FM_Key_Position;

typedef struct {
} gcc_pack FM_Point_Delta;

}
#ifdef WIN32
# pragma pack(pop)
#endif

class FlymasterGps : public Gps {
  public:
    FlymasterGps(SerialDev *pdev) { 
        dev = pdev; 
        try {
            init_gps();
        } catch (Exception e) {
            delete dev;
            throw e;
        }
    };
    virtual ~FlymasterGps() { delete dev; };
    
    /* Download tracklog from GPS */
    virtual PointArr download_tracklog(dt_callback cb, void *arg);

  private:
    SerialDev *dev;
    void init_gps();
    
    std::vector<std::string> send_command(const std::string &command, const std::vector<std::string> &parameters);
    std::string gen_command(const std::string &command, const std::vector<std::string> &parameters);
    std::vector<std::string> send_command(const std::string &command);
    void send_smpl_command(const std::string &command, const std::vector<std::string> &parameters);
    void send_smpl_command(const std::string &command);
    std::vector<std::string> receive_data(const std::string &command);
};

#endif