#include <string>
#include "../Object/User.h"
#include "../Object/Book.h"
class UserServlet{
    public:

        bool login(user User);
        bool regis(user User);
        static UserServlet& getInstance();
    private:
        UserServlet(){}
        ~UserServlet(){}
};
class BookServlet{
    public:
        void getBookInfo(Page &page);
        static BookServlet& getInstance();
    private:
        BookServlet(){}
        ~BookServlet(){}
};