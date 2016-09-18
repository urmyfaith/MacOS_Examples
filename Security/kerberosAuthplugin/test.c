
#include <unistd.h>
#include <readpassphrase.h>

#include "dir_svc_client.h"
#include "krb5-auth.h"


typedef enum {
	kerberos,
	directoryservice
} authentication_type;

int usage(int argc, char *argv[])
{
	fprintf(stderr, "%s [-u username] [-p principal] [-kK]\n", argv[0]);
	fprintf(stderr, "	-k authenticate using kerberos (default DirectoryService)\n");
	fprintf(stderr, "	-K authenticate using kerberos and authenticate kdc\n\n");
	exit(0);
}

CFArrayRef find_user_records_by_authauthority(KADirectoryNode *node, const char *authauthority)
{
	// should be eDSStartsWith such that we can 
	// find all entries and not key off opt parameters
	return find_records_by_attribute_value(node, kDSStdRecordTypeUsers, kDSNAttrAuthenticationAuthority, eDSiExact, authauthority, kDSAttributesAll);
}

int main (int argc, char *argv[])
{
	authentication_type authtype = directoryservice;
	bool verify = false;
	const char *username = NULL, *principal = NULL;
	char password[80];
	int ch = -1;

	while ((ch = getopt(argc, argv, "u:p:kK")) != -1) 
	{
		switch (ch) 
		{
			case 'u': username = optarg;
				break;
			case 'p': principal = optarg;
				break;
			case 'K': verify = true;
			case 'k': authtype = kerberos;
				break;
			case '?':
			default:
				usage(argc, argv);
				exit(0);
		}
	}
	argc -= optind;
	argv += optind;
	
	if (!username)
		username = getlogin();
	
	if (!readpassphrase("Password: ", password, sizeof(password), 0))
	{
		fprintf(stderr, "Failed to get passphrase - abort.\n");
		exit(1);
	}

	if (authtype == directoryservice)
		fprintf(stdout, "ds authentication for user %s %s.\n", username, (eDSNoErr == checkpw(username, password)) ? "successful" : "failed");
	else if (authtype == kerberos)
		fprintf(stdout, "kerberos authentication for user %s %s.\n", username, (0 ==  KLoginPrincipal(principal, username, password, verify)) ? "successful" : "failed");
	else 
		fprintf(stderr, "noimp\n");
		
	return 0;
}

/*
 IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. ("Apple") in
 consideration of your agreement to the following terms, and your use, installation,
 modification or redistribution of this Apple software constitutes acceptance of these
 terms.  If you do not agree with these terms, please do not use, install, modify or
 redistribute this Apple software.

 In consideration of your agreement to abide by the following terms, and subject to these
 terms, Apple grants you a personal, non-exclusive license, under Apple's copyrights in
 this original Apple software (the "Apple Software"), to use, reproduce, modify and
 redistribute the Apple Software, with or without modifications, in source and/or binary
 forms; provided that if you redistribute the Apple Software in its entirety and without
 modifications, you must retain this notice and the following text and disclaimers in all
 such redistributions of the Apple Software.  Neither the name, trademarks, service marks
 or logos of Apple Computer, Inc. may be used to endorse or promote products derived from
 the Apple Software without specific prior written permission from Apple. Except as expressly
 stated in this notice, no other rights or licenses, express or implied, are granted by Apple
 herein, including but not limited to any patent rights that may be infringed by your
 derivative works or by other works in which the Apple Software may be incorporated.

 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES,
 EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
 USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.

 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
          OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
 REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR
 OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
