/* Simple get functions for CaosDB for use from other languages.
 * A. Schlemmer, 04/2019
 */

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <curl/curl.h>
#include <string>
#include <iostream>
#include <cstdlib>
#include <fstream>

#include <string.h>

using namespace std;

#define CURLCODE_ERRORCHECK(function_name) if (code != CURLE_OK) {\
    return "Error: Error setting libCURL options (function_name)."; }

#define CURL_PERFORM(function_name) char errbuf[CURL_ERROR_SIZE];\
  errbuf[0] = 0;\
  code = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);\
  CURLCODE_ERRORCHECK(caosdb_login)\
  code = curl_easy_perform(curl);\
  if (code != CURLE_OK) {\
    return "Error: Error in libCURL perform (caosdb_login). [" + string(errbuf) + "]";\
  }\
  curl_easy_cleanup(curl);\
  curl_global_cleanup();\
  

/*
 * Write function used as handler for curl for response streams
 * and header response streams.
 */
extern "C" size_t writefunc(char *p, size_t size, size_t nm, string *stream) {
  stream->append(p, nm * size); 
  return nm * size;
}

/*
 * Initialize curl.
 * This function returns:
 * 0 if everything went ok
 * 1 for failures during intialization
 * 2 for other failures
 */
int init(CURL *&curl, string url, string ca) {
  CURLcode code;
  code = curl_global_init(CURL_GLOBAL_ALL); // needed for windows

  if (code != CURLE_OK) {
    return 1;
  }
  
  curl = curl_easy_init();
  if (!curl) {
    return 1;
  }
  
  code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, *writefunc);
  if (code != CURLE_OK) {
    return 2;
  }
  code = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, *writefunc);
  if (code != CURLE_OK) {
    return 2;
  }
  
  code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  if (code != CURLE_OK) {
    return 2;
  }
  code = curl_easy_setopt(curl, CURLOPT_CAINFO, ca.c_str());
  if (code != CURLE_OK) {
    return 2;
  }

  return 0;
}

/*
 * Setup the data pointers used for the response and header stream retrieval
 * function handlers defined above.
 *
 * The function returns 0 on success and 2 otherwise.
 */
int set_streams(CURL *&curl, string *stream, string *headerstream) {
  CURLcode code;
  code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, stream);
  if (code != CURLE_OK) {
    return 2;
  }
  code = curl_easy_setopt(curl, CURLOPT_HEADERDATA, headerstream);
  if (code != CURLE_OK) {
    return 2;
  }

  return 0;
}

/*
 * Login function for CaosDB.
 *
 * @param user The url starting from the baseurl domain. Example: "Entity/101"
 * @param pw The cookiestring which you obtained by calling caosdb_login.
 * @param baseurl The base url of your server. Example: "https://localhost:8887/playground/"
 * @param cacert The path to a certificate pem file. If left empty no custom certificate will be used.
 * @param verbose Instruct cURL to be verbose.
 * @return The response body from the server or an error message starting with "Error: ..." in case of errors.
 */
string caosdb_login(string user, string pw, string baseurl, string cacert, bool verbose) {
  CURL *curl;
  CURLcode code;
  int rv;  // return values for caoslib functions
  string content, header;

  rv = init(curl, baseurl + "login", cacert);
  if (rv == 1) {
    return "Error: Error during libCURL initialization (init).";
  } else if (rv == 2) {
    return "Error: Error setting libCURL options (init).";
  }
  rv = set_streams(curl, &content, &header);
  if (rv == 2) {
    return "Error: Error setting libCURL stream options (set_streams).";
  }

  string inpline = "username=" + user + "&password=" + pw;

  code = curl_easy_setopt(curl, CURLOPT_POST, 1L);
  CURLCODE_ERRORCHECK(caosdb_login)
  code = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, inpline.c_str());
  CURLCODE_ERRORCHECK(caosdb_login)

  if (verbose) {
    code = curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    CURLCODE_ERRORCHECK(caosdb_login)
  }

  CURL_PERFORM(caosdb_login)

  int cookpos = header.find("Set-Cookie: ");
  if (cookpos == string::npos) {
    if (header.find("401 Unauthorized") != string::npos) {
      return "Error: Authentication failed.";
    }
    return "Error: The server returned no cookie.";
  }

  // Extract the cookie from the response header:
  string cookiestring;
  int rscook = cookpos + 12;
  int endcook = header.find("\n", rscook);
  cookiestring = header.substr(rscook, endcook - rscook - 1);

  return cookiestring;
}

/*
 * Generic CaosDB get function.
 *
 * This can be used to retrieve entities, information or issue queries to CaosDB.
 *
 * @param url The url starting from the baseurl domain. Example: "Entity/101"
 * @param cookiestring The cookiestring which you obtained by calling caosdb_login.
 * @param baseurl The base url of your server. Example: "https://localhost:8887/playground/"
 * @param cacert The path to a certificate pem file. If left empty no custom certificate will be used.
 * @param verbose Instruct cURL to be verbose.
 * @return The response body from the server or an error message starting with "Error: ..." in case of errors.
 */
string caosdb_get(string url, string cookiestring, string baseurl, string cacert, bool verbose) {
  CURL *curl;
  CURLcode code;
  int rv;  // return values for caoslib functions
  string content, header;
  
  rv = init(curl, baseurl + url, cacert);
  if (rv == 1) {
    return "Error: Error during libCURL initialization (init).";
  } else if (rv == 2) {
    return "Error: Error setting libCURL options (init).";
  }
  rv = set_streams(curl, &content, &header);
  if (rv == 2) {
    return "Error: Error setting libCURL stream options (set_streams).";
  }

  if (verbose) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    CURLCODE_ERRORCHECK(caosdb_get)
  }

  curl_easy_setopt(curl, CURLOPT_COOKIE, cookiestring.c_str());
  CURLCODE_ERRORCHECK(caosdb_get)

  CURL_PERFORM(caosdb_get)

  return content;
}

/*
 * Generic CaosDB delete function.
 *
 * @param url The url starting from the baseurl domain. Example: "Entity/101"
 * @param cookiestring The cookiestring which you obtained by calling caosdb_login.
 * @param baseurl The base url of your server. Example: "https://localhost:8887/playground/"
 * @param cacert The path to a certificate pem file. If left empty no custom certificate will be used.
 * @param verbose Instruct cURL to be verbose.
 * @return The response body from the server or an error message starting with "Error: ..." in case of errors.
 */
string caosdb_delete(string url, string cookiestring, string baseurl, string cacert, bool verbose) {
  CURL *curl;
  CURLcode code;
  int rv;  // return values for caoslib functions
  string content, header;
  
  rv = init(curl, baseurl + url, cacert);
  if (rv == 1) {
    return "Error: Error during libCURL initialization (init).";
  } else if (rv == 2) {
    return "Error: Error setting libCURL options (init).";
  }
  rv = set_streams(curl, &content, &header);
  if (rv == 2) {
    return "Error: Error setting libCURL stream options (set_streams).";
  }

  if (verbose) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    CURLCODE_ERRORCHECK(caosdb_delete)
  }

  curl_easy_setopt(curl, CURLOPT_COOKIE, cookiestring.c_str());
  CURLCODE_ERRORCHECK(caosdb_delete)
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
  CURLCODE_ERRORCHECK(caosdb_delete)

  CURL_PERFORM(caosdb_delete)

  return content;
}


/*
 * Generic CaosDB put function.
 *
 * 
 *
 * @param url The url starting from the baseurl domain. Example: "Entity/101"
 * @param cookiestring The cookiestring which you obtained by calling caosdb_login.
 * @param body The body to be sent to CaosDB. This is an XML document conforming with the CaosDB XML API.
 * @param baseurl The base url of your server. Example: "https://localhost:8887/playground/"
 * @param cacert The path to a certificate pem file. If left empty no custom certificate will be used.
 * @param verbose Instruct cURL to be verbose.
 * @return The response body from the server or an error message starting with "Error: ..." in case of errors.
 */
string caosdb_put(string url, string cookiestring, string body, string baseurl, string cacert, bool verbose) {
  CURL *curl;
  CURLcode code;
  int rv;  // return values for caoslib functions
  string content, header;
  
  rv = init(curl, baseurl + url, cacert);
  if (rv == 1) {
    return "Error: Error during libCURL initialization (init).";
  } else if (rv == 2) {
    return "Error: Error setting libCURL options (init).";
  }
  rv = set_streams(curl, &content, &header);
  if (rv == 2) {
    return "Error: Error setting libCURL stream options (set_streams).";
  }

  if (verbose) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    CURLCODE_ERRORCHECK(caosdb_put)
  }

  curl_easy_setopt(curl, CURLOPT_COOKIE, cookiestring.c_str());
  CURLCODE_ERRORCHECK(caosdb_put)

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
  CURLCODE_ERRORCHECK(caosdb_put)
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  CURLCODE_ERRORCHECK(caosdb_put)

  CURL_PERFORM(caosdb_put)
  
  return content;
}

/*
 * Generic CaosDB post function.
 *
 * @param url The url starting from the baseurl domain. Example: "Entity/101"
 * @param cookiestring The cookiestring which you obtained by calling caosdb_login.
 * @param body The body to be sent to CaosDB. This is an XML document conforming with the CaosDB XML API.
 * @param baseurl The base url of your server. Example: "https://localhost:8887/playground/"
 * @param cacert The path to a certificate pem file. If left empty no custom certificate will be used.
 * @param verbose Instruct cURL to be verbose.
 * @return The response body from the server or an error message starting with "Error: ..." in case of errors.
 */
string caosdb_post(string url, string cookiestring, string body, string baseurl, string cacert, bool verbose) {
  CURL *curl;
  CURLcode code;
  int rv;  // return values for caoslib functions
  string content, header;
  
  rv = init(curl, baseurl + url, cacert);
  if (rv == 1) {
    return "Error: Error during libCURL initialization (init).";
  } else if (rv == 2) {
    return "Error: Error setting libCURL options (init).";
  }
  rv = set_streams(curl, &content, &header);
  if (rv == 2) {
    return "Error: Error setting libCURL stream options (set_streams).";
  }

  if (verbose) {
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    CURLCODE_ERRORCHECK(caosdb_post)
  }

  curl_easy_setopt(curl, CURLOPT_COOKIE, cookiestring.c_str());
  CURLCODE_ERRORCHECK(caosdb_post)

  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
  CURLCODE_ERRORCHECK(caosdb_post)
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  CURLCODE_ERRORCHECK(caosdb_post)
  
  CURL_PERFORM(caosdb_post)

  return content;
}


/*
 * Utility function for retrieving a password from the command line
 * password manager "pass".
 *
 * @param pw_identifier is the identifier passed to "pass"
 * @return the password in plain text
 */
string get_pass_pw(string pw_identifier) {
  FILE *pstream;
  string result;
  pstream = popen(("pass " + pw_identifier).c_str(), "r");
  if (pstream != NULL) {
    char buf[2000];
    result = string(fgets(buf, sizeof(buf), pstream));
    pclose(pstream);
  }
  return result;
}

/* -------------------------------------------- */
/* Section for interface and testing functions. */
/* -------------------------------------------- */

/*
 * This function wraps the caosdb_login function declared above for use in
 * C-libraries. Parameters and return value are the same, except that the
 * datatypes are changed to pointers to char arrays.
 */
extern "C" char *login(const char *username, const char *password,
                       const char *baseurl, const char *cacert, bool verbose) {
  string str = caosdb_login(string(username), string(password),
                            string(baseurl), string(cacert), verbose);
  char* str2 = (char*)malloc(str.length()+1);
  strcpy(str2, str.c_str());
  return str2;
}

/*
 * This function wraps the caosdb_get function declared above for use in
 * C-libraries. Parameters and return value are the same, except that the
 * datatypes are changed to pointers to char arrays.
 */
extern "C" char *get(const char *url, const char *cookiestr,
                     const char *baseurl, const char *cacert, bool verbose) {
  string str = caosdb_get(string(url), string(cookiestr),
                          string(baseurl), string(cacert), verbose);
  char* str2 = (char*)malloc(str.length()+1);
  strcpy(str2, str.c_str());
  return str2;
}

/*
 * This function wraps the caosdb_delete function declared above for use in
 * C-libraries. Parameters and return value are the same, except that the
 * datatypes are changed to pointers to char arrays.
 */
extern "C" char *del(const char *url, const char *cookiestr,
                     const char *baseurl, const char *cacert, bool verbose) {
  string str = caosdb_delete(string(url), string(cookiestr),
                             string(baseurl), string(cacert), verbose);
  char* str2 = (char*)malloc(str.length()+1);
  strcpy(str2, str.c_str());
  return str2;
}

/*
 * This function wraps the caosdb_put function declared above for use in
 * C-libraries. Parameters and return value are the same, except that the
 * datatypes are changed to pointers to char arrays.
 */
extern "C" char *put(const char *url, const char *cookiestr, const char *body,
                     const char *baseurl, const char *cacert, bool verbose) {
  string str = caosdb_put(string(url), string(cookiestr), string(body),
                          string(baseurl), string(cacert), verbose);
  char* str2 = (char*)malloc(str.length()+1);
  strcpy(str2, str.c_str());
  return str2;
}

/*
 * This function wraps the caosdb_post function declared above for use in
 * C-libraries. Parameters and return value are the same, except that the
 * datatypes are changed to pointers to char arrays.
 */
extern "C" char *post(const char *url, const char *cookiestr, const char *body,
                      const char *baseurl, const char *cacert, bool verbose) {
  string str = caosdb_post(string(url), string(cookiestr), string(body),
                           string(baseurl), string(cacert), verbose);
  char* str2 = (char*)malloc(str.length()+1);
  strcpy(str2, str.c_str());
  return str2;
}

/*
 * This function wraps the get_pass_pw function declared above for use in
 * C-libraries. Parameters and return value are the same, except that the
 * datatypes are changed to pointers to char arrays.
 */
extern "C" char *pass_pw(const char *pw_identifier) {
  string str = get_pass_pw(string(pw_identifier));
  char* str2 = (char*)malloc(str.length()+1);
  strcpy(str2, str.c_str());
  return str2;
}


