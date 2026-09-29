#ifndef _LED_H_
#define _LED_H_

class Led {
public:
    virtual ~Led() = default;
    virtual void OnStateChanged() = 0;

    // Optional status hooks. Boards that do not need them keep the no-op defaults.
    virtual void OnWifiLost() {}
    virtual void OnServerLost() {}
    virtual void OnProcessing(bool active) {}
};

class NoLed : public Led {
public:
    void OnStateChanged() override {}
};

#endif // _LED_H_
