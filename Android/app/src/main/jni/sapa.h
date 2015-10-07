#include <stdio.h>
#include <IAPAInterface.h>
#include "sapaClient.h"
#include "APACommon.h"

namespace android {

    class APAWave: IAPAInterface { // The main interface to Samsung Professional Audio SDK.
    public:
        APAWave();
        virtual ~APAWave();

        int init();
        int sendCommand(const char *command);
        IJackClientInterface *getJackClientInterface();
        int request(const char *what, const long ext1, const long capacity, size_t &len, void *data);

    private:
        sapaClient client;
    };
    
    DECLARE_APA_INTERFACE(APAWave)

};
