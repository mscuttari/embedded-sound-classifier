#ifndef BUTTON_H
#define BUTTON_H

class UserButton {
public:

    UserButton() = delete;

    /**
     * Wait for the user to click the button.
     * The calling thread is paused.
     */
    static void wait();

private:

    /**
     * Initialize the user button peripheral
     */
    static void initialize();
};

#endif /* BUTTON_H */
