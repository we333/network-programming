CC		=	g++
SRCS	= 	main.cpp	\
			worker.cpp	\
			utility.cpp

OBJS	=	$(SRCS:.cpp=.o)
EXEC	=	main
LIBS	=	-lmysqlcppconn
#LIBS	=	-lmysqlcppconn -lpthread

start:	$(OBJS)
	$(CC) -o $(EXEC) $(OBJS) $(LIBS)
.cpp.o:
	$(CC) -o $@ -c $<

clean:
	rm -rf $(OBJS)