#include <my_config.h>
#include "testconnections.h"
#include "sql_t1.h"
#include "maxadmin_operations.h"

char sql[1000000];

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
int exit_flag = 0;
void *query_thread( void *ptr );
void *checks_thread( void *ptr);

TestConnections * Test;

int main()
{

    Test = new TestConnections();
    int global_result = 0;

    Test->ReadEnv();
    Test->PrintIP();
    Test->repl->Connect();
    Test->ConnectRWSplit();

    // connect to the MaxScale server (rwsplit)

    if (Test->conn_rwsplit == NULL ) {
        printf("Can't connect to MaxScale\n");
        exit(1);
    } else {

        create_t1(Test->conn_rwsplit);

        create_insert_string(sql, 50000, 1);
        printf("sql_len=%lu\n", strlen(sql));
        global_result += execute_query(Test->conn_rwsplit, sql);

        pthread_t threads[1000];
        pthread_t check_thread;
        int  iret[1000];
        int check_iret;
        int j;
        exit_flag=0;
        /* Create independent threads each of them will execute function */
        for (j=0; j<10; j++) {
            iret[j] = pthread_create( &threads[j], NULL, query_thread, NULL);
        }
        check_iret = pthread_create( &check_thread, NULL, checks_thread, NULL);

        /*for (j=0; j<10; j++) {
        pthread_join( threads[j], NULL);
     }
     pthread_join(check_thread, NULL);*/

        char result[1024];
        for (int i = 0; i < 1000; i++) {
            getMaxadminParam(Test->Maxscale_IP, (char *) "admin", (char *) "skysql", (char *) "show server server2", (char *) "Slave delay:", result);
            printf("server2: %s\n");
            getMaxadminParam(Test->Maxscale_IP, (char *) "admin", (char *) "skysql", (char *) "show server server3", (char *) "Slave delay:", result);
            printf("server3: %s\n");
            getMaxadminParam(Test->Maxscale_IP, (char *) "admin", (char *) "skysql", (char *) "show server server4", (char *) "Slave delay:", result);
            printf("server4: %s\n");
        }
        exit_flag = 1;
        // close connections
        Test->CloseRWSplit();
    }
    Test->repl->CloseConn();

    exit(global_result);
}


void *query_thread( void *ptr )
{
    MYSQL *conn;

    conn = Test->OpenRWSplitConn();
    while (exit_flag == 0) {
        execute_query(conn, sql);
    }

    mysql_close(conn);
    return NULL;
}

void *checks_thread( void *ptr )
{
    int i;
    int j;
    for (i=0; i<1000000; i++) {
        printf("i=%u\t ", i);
        for (j=0; j < Test->repl->N; j++) {printf("SBM=%u\t", get_Seconds_Behind_Master(Test->repl->nodes[j]));}
        printf("\n");
    }
    exit_flag=1;
    return NULL;
}

