##The actual awesome "Libtask" by Russ Cox

###Whats modified
-Added Multithreading support.
-Added Epoll instead of poll
-Removed debugging information by using build flag -DVERSION_DEBUG
-Ability to hook system calls (read, write, accept ..(more to come) ) to make your existing libraries like libSSL, mongo and mysql run on co-routines.
-More comments/documentation


