#ifndef BUTTON_H
#define BUTTON_H

#include <miosix.h>

using namespace miosix;

class UserButton {
public:
    /**
     * Get a singleton instance
     */
    static UserButton& getInstance();
    
    /** Avoid copies */
    UserButton(UserButton const&) = delete;
    void operator = (UserButton const&) = delete;
    
    /**
     * Block the calling thread until the button has been pressed
     */
    void wait();
    
private:
    /** Singleton */
    UserButton();
    static UserButton *instance;
};

#endif /* BUTTON_H */
