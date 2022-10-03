Copyright Teodora Stroe 321CA 2022

# Web Client

Implementation of a web client that interacts with a __REST API__, focusing on the _HTTP communication mechanisms_ and the usage of external libraries for the _handling of JSON objects_.

## Commands

### register

- COMMAND FLOW:

    - receives the credentials from the user;
    - creates the JSON object containing the data;
    - sends POST request to the server.

- POSSIBLE ERRORS:

    - the credentials contain incorrect characters;
    - the user is currently logged in;
    - the username is already in use.

### login

- COMMAND FLOW:

    - receives the credentials from the user;
    - creates the JSON object containing the data;
    - sends POST request to the server;
    - saves the login cookie from the server response.

- POSSIBLE ERRORS:

    - the credentials contain incorrect characters;
    - there is already another session in use;
    - the credentials do not match with any existing accounts.

### logout

- COMMAND FLOW:

    - sends GET request to the server;
    - deletes the session's login cookie and the library token.

- POSSIBLE ERRORS:

    - there is no session in use.

### enter_library

- COMMAND FLOW:

    - sends GET request to the server;
    - saves the JWT token from the server response.

- POSSIBLE ERRORS:

    - the user is not logged in.

### get_books

- COMMAND FLOW:

    - sends GET request to the server;
    - saves the JSON from the server response;
    - brief display of the books in the library. 

- POSSIBLE ERRORS:

    - the user does not have access to the library.

### get_book

- COMMAND FLOW:

    - sends GET request to the server;
    - saves the JSON from the server response;
    - detailed display of the requested book. 

- POSSIBLE ERRORS:

    - the given ID is not a number;
    - the user doesn't have access to the library;
    - the library does not contain a book with the given ID.

### add_book

- COMMAND FLOW:

    - creates the JSON object containing the data;
    - sends POST request to the server.

- POSSIBLE ERRORS:

    - the number of pages is not a number;
    - the user doesn't have access to the library.

### delete_book

- COMMAND FLOW:

    - sends DELETE request to the server.

- POSSIBLE ERRORS:

    - the given ID is not a number;
    - the user doesn't have access to the library;
    - the library does not contain a book with the given ID.

### exit

- shuts down the client.

## HTTP requests

The functions used to create request messages are based on the lab10 skel files [1], to which were added _authentication header_ options.

   - GET: compute_get_request(...);
   - POST: compute_post_request(...);
   - DELETE: compute_delete_request(...).

## JSON parsing

For parsing JSON objects, the client uses the `parson` library, provided by kgabis [2].
This decision was based on the clean documentation of the project and the easy
way to interact with JSON objects and values using the C language.

## Error handling
Input errors are handled locally by the client.

Registration / authentication / other types of error handling is based on the
response received from the server to ensure  the command has the desired effect.

Because of the input checking methods that were used, the client does not accept 0 as a valid numerical value (ID, number of pages).

## Simulation

`Postman` [3] was used for simulating the client's correct behaviour. 

## References

[1]. https://ocw.cs.pub.ro/courses/pc/laboratoare/10

[2]. https://github.com/kgabis/parson

[3]. https://www.postman.com/