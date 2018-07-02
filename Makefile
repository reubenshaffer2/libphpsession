all: libphpsession.la sess_test

clean:
	rm *.o *.lo *.la .libs/* sess_test

libphpsession.la: php_session.lo
	libtool --tag=CC --mode=link gcc -g -O -pthread -o libphpsession.la php_session.lo -rpath /usr/local/lib

php_session.lo: php_session.c php_session.h
	libtool --tag=CC --mode=compile gcc -g -I.. -O -pthread -c php_session.c

sess_test: sess_test.c php_session.c php_session.h
	gcc -o sess_test sess_test.c php_session.c
