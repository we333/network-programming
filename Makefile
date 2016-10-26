CC		=	g++
SRCS	= 	server.cpp

OBJS	=	$(SRCS:.cpp=.o)
EXEC	=	server
LIBS	=	-lmysqlcppconn -lpthread

start:	$(OBJS)
	$(CC) -o $(EXEC) $(OBJS) $(LIBS)
.cpp.o:
	$(CC) -o $@ -c $<

clean:
	rm -rf $(OBJS)