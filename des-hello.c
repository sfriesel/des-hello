#include <dessert.h>
#include <dessert-extra.h>
#include <string.h>
#include <time.h>

#define EXPLODE_ARRAY6( ARRAY ) ARRAY[0], ARRAY[1], ARRAY[2], ARRAY[3], ARRAY[4], ARRAY[5]
#define HELLO_EXT_TYPE DESSERT_EXT_USER + 4

int periodic_send_hello(void *data, struct timeval *scheduled, struct timeval *interval) {
	dessert_info("sending hello\n");
	dessert_msg_t* hello_msg;
	dessert_ext_t* ext;

	// create new HELLO message with hello_ext.
	dessert_msg_new(&hello_msg);
	hello_msg->ttl = 2;

	dessert_msg_addext(hello_msg, &ext, HELLO_EXT_TYPE, 0);

	if(dessert_meshsend(hello_msg, NULL) != DESSERT_OK) {
		printf("FAIL\n");
	}
	dessert_msg_destroy(hello_msg);
	return 0;
}

int handle_hello(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, const dessert_meshif_t *iface, dessert_frameid_t id){
	dessert_ext_t* hallo_ext;

	if (dessert_msg_getext(msg, &hallo_ext, HELLO_EXT_TYPE, 0) != 0) {
		msg->ttl--;
		if (msg->ttl >= 1) { // send hello msg back
			memcpy(msg->l2h.ether_dhost, msg->l2h.ether_shost, ETH_ALEN);
			dessert_meshsend(msg, iface);
		} else {
			if (memcmp(iface->hwaddr, msg->l2h.ether_dhost, ETH_ALEN) == 0) {
				struct timeval ts;
				gettimeofday(&ts, NULL);
				//aodv_db_cap2Dneigh(msg->l2h.ether_shost, iface, &ts);
				dessert_info("received hello resp from %x:%x:%x:%x:%x:%x\n", EXPLODE_ARRAY6(msg->l2h.ether_shost));
			}
		}
		return DESSERT_MSG_DROP;
	}
	return DESSERT_MSG_KEEP;
}

/**
* Send dessert message via all registered interfaces.
**/
int toMesh(dessert_msg_t* msg, size_t len, dessert_msg_proc_t *proc, dessert_sysif_t *tunif, dessert_frameid_t id) {
	dessert_meshsend(msg, NULL);
	return DESSERT_MSG_DROP;
}

int main(int argc, char *argv[]) {
	FILE *cfg = dessert_cli_get_cfg(argc, argv);
	//FILE *cfg = fopen("/etc/default/des-aodv", "r");
	dessert_init("DESX", 0xEE, DESSERT_OPT_NODAEMONIZE);
	
	
	/* initalize logging */
	dessert_logcfg(DESSERT_LOG_STDERR);
	
	/* cli initialization */
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "sys", dessert_cli_cmd_addsysif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize sys interface");
	cli_register_command(dessert_cli, dessert_cli_cfg_iface, "mesh", dessert_cli_cmd_addmeshif, PRIVILEGE_PRIVILEGED, MODE_CONFIG, "initialize mesh interface");

	/* registering callbacks */
	dessert_meshrxcb_add(dessert_msg_ifaceflags_cb, 20);
	dessert_meshrxcb_add(handle_hello, 40);

	dessert_sysrxcb_add(toMesh, 100);

	/* registering periodic tasks */
	struct timeval hello_interval_t;
	hello_interval_t.tv_sec = 1;
	hello_interval_t.tv_usec = 0;
	dessert_periodic_add(periodic_send_hello, NULL, NULL, &hello_interval_t);

	/* running cli & daemon */
	cli_file(dessert_cli, cfg, PRIVILEGE_PRIVILEGED, MODE_CONFIG);
	dessert_cli_run();
	dessert_run();

	return EXIT_SUCCESS;
}
