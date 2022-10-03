#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <stdbool.h>    /* booleans */
#include <time.h>
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"
#include "parson.h"

// server info
#define HOST                "34.241.4.235"
#define PORT                8080

// commands
#define REGISTER            "register"
#define LOGIN               "login"
#define ENTER_LIB           "enter_library"
#define GET_BOOKS           "get_books"
#define GET_BOOK            "get_book"
#define ADD_BOOK            "add_book"
#define DELETE_BOOK         "delete_book"
#define LOGOUT              "logout"
#define EXIT                "exit"

// paths
#define REGISTER_PATH       "/api/v1/tema/auth/register"
#define LOGIN_PATH          "/api/v1/tema/auth/login"
#define LOGOUT_PATH         "/api/v1/tema/auth/logout"
#define LIB_ACCESS_PATH     "/api/v1/tema/library/access"
#define BOOKS_PATH          "/api/v1/tema/library/books"

// Content Type
#define JSON_CONTENT        "application/json"

// errors
#define ERROR               "error"
#define ERR_AUTHORIZATION   "Authorization"
#define ERR_BOOK            "No book"

// cookie
#define COOKIE_START        "connect.sid"
#define COOKIE_LEN          96
#define MAX_COOKIE_LEN      100

// token
#define TOKEN_START         "token"
#define TOKEN_LEN           500
#define TOKEN_PREFIX        "Bearer "

// size
#define MAX_LEN             50

// user info
#define USERNAME            "username"
#define PASSWORD            "password"

// book info
#define ID                  "id"
#define TITLE               "title"
#define AUTHOR              "author"
#define GENRE               "genre"
#define PUBLISHER           "publisher"
#define PAGE_COUNT          "page_count"

// others
#define JSON_ARRAY_START    "["

// functions
char *book_path(char *id);
void print_book_brief(JSON_Object *book);
void print_book(JSON_Object *book);

int main(int argc, char *argv[]) {
    int sockfd;

    // alloc login cookie
    char *login_cookie = calloc(MAX_COOKIE_LEN, sizeof(*login_cookie));
    bool logged_in = false;

    // alloc JWT token
    char *token = calloc(TOKEN_LEN, sizeof(*token));
    bool active_token = false;

    while (true) {
        char cmd[MAX_LEN];
        scanf(" %50[^\n]", cmd);

        if (!strcmp(cmd, REGISTER)) {
            char username[MAX_LEN], password[MAX_LEN];
            char *request;
            char *response;
            char *err;
            char *space_username, *space_psswd;

            printf("%s = ", USERNAME);
            scanf(" %50[^\n]", username);

            printf("%s = ", PASSWORD);
            scanf(" %50[^\n]", password);

            space_username = strstr(username, " ");
            space_psswd = strstr(password, " ");

            if (space_username || space_psswd) {
                printf("\n> The credentials must not contain any spaces!\n\n");
                continue;
            }

            if (logged_in) {
                printf("\n> You are already logged in! Log out and try again.\n\n");
                continue;
            }

            // create JSON input
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            char *json_string = NULL;
            json_object_set_string(root_object, USERNAME, username);
            json_object_set_string(root_object, PASSWORD, password);
            json_string = json_serialize_to_string(root_value);

            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
            request = compute_post_request(HOST, REGISTER_PATH, JSON_CONTENT, &json_string, 1, NULL, 0, NULL, 0);
            send_to_server(sockfd, request);
            response = receive_from_server(sockfd);
            close(sockfd);

            err = strstr(response, ERROR);
            if (err) {
                printf("\n> Sorry... username already taken! Try something else.\n\n");
            } else {
                printf("\nYou registered successfully!\n\n");
            }
            
            json_free_serialized_string(json_string);
            json_value_free(root_value);
        } else if (!strcmp(cmd, LOGIN)) {
            char username[MAX_LEN], password[MAX_LEN];
            char *request;
            char *response;
            char *err;
            char *space_username, *space_psswd;

            printf("%s = ", USERNAME);
            scanf(" %50[^\n]", username);

            printf("%s = ", PASSWORD);
            scanf(" %50[^\n]", password);

            space_username = strstr(username, " ");
            space_psswd = strstr(password, " ");

            if (space_username || space_psswd) {
                printf("\n> The credentials must not contain any spaces!\n\n");
                continue;
            }

            if (logged_in) {
                printf("\n> You are already logged in! Log out and try again.\n\n");
                continue;
            }

            // create JSON input
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            char *json_string = NULL;
            json_object_set_string(root_object, USERNAME, username);
            json_object_set_string(root_object, PASSWORD, password);
            json_string = json_serialize_to_string(root_value);

            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
            request = compute_post_request(HOST, LOGIN_PATH, JSON_CONTENT, &json_string, 1, NULL, 0, NULL, 0);
            send_to_server(sockfd, request);
            response = receive_from_server(sockfd);
            close(sockfd);

            err = strstr(response, ERROR);
            if (err) {
                printf("\n> The credentials are incorrect!\n\n");
            } else {
                printf("\n> You are logged in!\n\n");

                logged_in = true;

                // select cookie from server response
                char* start_cookie = strstr(response, COOKIE_START);

                // save the login cookie
                memcpy(login_cookie, start_cookie, COOKIE_LEN);
            }

            json_free_serialized_string(json_string);
            json_value_free(root_value);
        } else if (!strcmp(cmd, LOGOUT)) {
            char *request;
            char *response;
            char *err;

            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
            request = compute_get_request(HOST, LOGOUT_PATH, NULL, NULL, 0, &login_cookie, 1);
            send_to_server(sockfd, request);
            response = receive_from_server(sockfd);
            close(sockfd);

            err = strstr(response, ERROR);
            if (err) {
                printf("\n> You are NOT logged in!\n\n");
            } else {
                printf("\n> You are logged out!\n\n");
                logged_in = false;

                // invalidate login cookie
                memset(login_cookie, 0, COOKIE_LEN);

                // invalidate JWT token
                if (active_token) {
                    memset(token, 0, TOKEN_LEN);
                    active_token = false;
                }
            }
        } else if (!strcmp(cmd, ENTER_LIB)) {
            char *request;
            char *response;
            char *err;

            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
            request = compute_get_request(HOST, LIB_ACCESS_PATH, NULL, NULL, 0, &login_cookie, 1);
            send_to_server(sockfd, request);
            response = receive_from_server(sockfd);
            close(sockfd);

            err = strstr(response, ERROR);
            if (err) {
                printf("\n> You are NOT logged in!\n\n");
                continue;
            }

            printf("\n> Access granted!\n\n");

            // select token from server response
            char *start_token = strstr(response, TOKEN_START);

            // save the token
            strcpy(token, TOKEN_PREFIX);
            strcat(token, start_token + 8);
            token[strlen(TOKEN_PREFIX) + strlen(start_token) - 10] = '\0';

            active_token = true;
        } else if (!strcmp(cmd, GET_BOOKS)) {
            char *request;
            char *response;
            char *err;

            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
            request = compute_get_request(HOST, BOOKS_PATH, NULL, &token, 1, NULL, 0);
            send_to_server(sockfd, request);
            response = receive_from_server(sockfd);
            close(sockfd);

            err = strstr(response, ERR_AUTHORIZATION);
            if (err) {
                printf("\n> You don't have access to the library! Enter the library first :)\n\n");
                continue;
            }

            // select JSON from server response
            char *start_json = strstr(response, JSON_ARRAY_START);

            JSON_Value *root_value = json_parse_string(start_json);
            JSON_Array *root_array = json_value_get_array(root_value);
            JSON_Object *json_book;
            
            printf("\n> Books currently in the library:\n\n");

            if (json_array_get_count(root_array) == 0) {
                printf("\t The library seems to be empty! Try adding some books.\n\n");
            } else {
                for (int i = 0; i < json_array_get_count(root_array); i++) {
                    json_book = json_array_get_object(root_array, i);
                    print_book_brief(json_book);
                }
            }
            
            json_value_free(root_value);
        } else if (!strcmp(cmd, GET_BOOK)) {
            char id_input[MAX_LEN];

            printf("%s = ", ID);
            scanf("%s", id_input);

            if (!atoi(id_input)) {
                printf("\n> You must enter a numeric ID, different from 0!\n\n");
                continue;
            }

            char *request;
            char *response;
            char *err;
            char *path = book_path(id_input);

            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);            
            request = compute_get_request(HOST, path, NULL, &token, 1, NULL, 0);
            send_to_server(sockfd, request);
            response = receive_from_server(sockfd);
            close(sockfd);

            err = strstr(response, ERR_AUTHORIZATION);
            if (err) {
                printf("\n> You don't have access to the library! Enter the library first :)\n\n");
                continue;
            }

            err = strstr(response, ERR_BOOK);
            if (err) {
                printf("\n> There is no book with this ID! Try adding it ;)\n\n");
                continue;
            }

            // select JSON from server response
            char *start_json = strstr(response, JSON_ARRAY_START);

            JSON_Value *root_value = json_parse_string(start_json);
            JSON_Array *root_array = json_value_get_array(root_value);
            JSON_Object *json_book;

            printf("\n");

            for (int i = 0; i < json_array_get_count(root_array); i++) {
                json_book = json_array_get_object(root_array, i);
                print_book(json_book);
            }
            
            json_value_free(root_value);
        } else if (!strcmp(cmd, ADD_BOOK)) {
            char title[MAX_LEN], author[MAX_LEN], genre[MAX_LEN], page_count[MAX_LEN], publisher[MAX_LEN];
            double number_pc;

            printf("%s = ", TITLE);
            scanf(" %50[^\n]", title);

            printf("%s = ", AUTHOR);
            scanf(" %50[^\n]", author);

            printf("%s = ", GENRE);
            scanf(" %50[^\n]", genre);

            printf("%s = ", PAGE_COUNT);
            scanf(" %50[^\n]", page_count);

            printf("%s = ", PUBLISHER);
            scanf(" %50[^\n]", publisher);

            number_pc = atof(page_count);

            if (!number_pc) {
                printf("\n> The number of pages must be a number, different from 0!\n\n");
                continue;
            }

            char *request;
            char *response;
            char *err;

            // create JSON input
            JSON_Value *root_value = json_value_init_object();
            JSON_Object *root_object = json_value_get_object(root_value);
            char *json_string = NULL;
            json_object_set_string(root_object, TITLE, title);
            json_object_set_string(root_object, AUTHOR, author);
            json_object_set_string(root_object, GENRE, genre);
            json_object_set_number(root_object, PAGE_COUNT, number_pc);
            json_object_set_string(root_object, PUBLISHER, publisher);
            json_string = json_serialize_to_string(root_value);

            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
            request = compute_post_request(HOST, BOOKS_PATH, JSON_CONTENT, &json_string, 1, &token, 1, NULL, 0);
            send_to_server(sockfd, request);
            response = receive_from_server(sockfd);
            close(sockfd);

            err = strstr(response, ERR_AUTHORIZATION);
            if (err) {
                printf("\n> You don't have access to the library! Enter the library first :)\n\n");
                continue;
            }

            printf("\n> Book added!\n\n");

            json_free_serialized_string(json_string);
            json_value_free(root_value);
        } else if (!strcmp(cmd, DELETE_BOOK)) {
            char id_input[MAX_LEN];

            printf("%s = ", ID);
            scanf("%s", id_input);

            if (!atoi(id_input)) {
                printf("\n> You must enter a numeric ID, different from 0!\n\n");
                continue;
            }

            char *request;
            char *response;
            char *err;
            char *path = book_path(id_input);

            sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);            
            request = compute_delete_request(HOST, path, &token, 1, NULL, 0);
            send_to_server(sockfd, request);
            response = receive_from_server(sockfd);
            close(sockfd);

            err = strstr(response, ERR_AUTHORIZATION);
            if (err) {
                printf("\n> You don't have access to the library! Enter the library first :)\n\n");
                continue;
            }

            err = strstr(response, ERR_BOOK);
            if (err) {
                printf("\n> There is no book with this ID!\n\n");
                continue;
            }

            printf("\n> Book deleted successfully!\n\n");
        } else if (!strcmp(cmd, EXIT)) {
            printf("\n> Bye!!!\n\n");
            break;
        } else {
            printf("\n> Invalid command!\n\n");
        }
    }

    if (logged_in) {
        free(login_cookie);
    }

    if (active_token) {
        free(token);
    }

    return 0;
}

char *book_path(char *id) {
    char *path = calloc(strlen(BOOKS_PATH) + MAX_LEN + 1, sizeof(*path));
    strcpy(path, BOOKS_PATH);
    strcat(path, "/");
    strcat(path, id);

    return path;
}

void print_book_brief(JSON_Object *book) {
    printf("\t%s:    %d\n", ID, (int)json_object_get_number(book, ID));
    printf("\t%s: %s\n", TITLE, json_object_get_string(book, TITLE));
    printf("\n");
}

void print_book(JSON_Object *book) {
    printf("\t%s:       %s\n", TITLE, json_object_get_string(book, TITLE));
    printf("\t%s:      %s\n", AUTHOR, json_object_get_string(book, AUTHOR));
    printf("\t%s:       %s\n", GENRE, json_object_get_string(book, GENRE));
    printf("\t%s:  %d\n", PAGE_COUNT, (int)json_object_get_number(book, PAGE_COUNT));
    printf("\t%s:   %s\n", PUBLISHER, json_object_get_string(book, PUBLISHER));
    printf("\n");
}