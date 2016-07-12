#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

// TODO: This works currently only on Linux

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

#include "blue.h"

// This is not really thread-safe
bool error;

int findConn(int s, int dev_id, long arg) {
	struct hci_conn_list_req *cl;
	struct hci_conn_info *ci;
	
	if ((cl = malloc(HCI_MAX_DEV * sizeof(*ci) + sizeof(*cl))) == NULL) {
		perror("malloc");
		error = true;
		return 0;
	}
	cl->dev_id = dev_id;
	cl->conn_num = HCI_MAX_DEV;
	ci = cl->conn_info;

	if (ioctl(s, HCIGETCONNLIST, (void *)cl)) {
		perror("Could not get connection list");
		error = true;
		free(cl);
		return 0;
	}

	int i;
	for (i = 0; i < cl->conn_num; i++, ci++) {
		if (!bacmp((bdaddr_t *)arg, &ci->bdaddr)) {
			free(cl);
			return 1;
		}
	}

	free(cl);
	return 0;
}

int blue_rssi(char *address) {
	struct hci_conn_info_req *cr;
	int8_t rssi;
	int dd, dev_id;
	
	bdaddr_t bdaddr;
	str2ba(address, &bdaddr);

	error = false;
	dev_id = hci_for_each_dev(HCI_UP, findConn, (long)&bdaddr);
	if (dev_id < 0) {
		if (error) {
			return BLUE_ERROR;
		}
		else {
			return BLUE_NOT_CONNECTED;
		}
	}

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("Could not open HCI device");
		return BLUE_ERROR;
	}

	if ((cr = malloc(sizeof(struct hci_conn_info_req) + sizeof(struct hci_conn_info))) == NULL) {
		perror("malloc");
		hci_close_dev(dd);
		return BLUE_ERROR;
	}

	bacpy(&cr->bdaddr, &bdaddr);
	cr->type = ACL_LINK;
	if (ioctl(dd, HCIGETCONNINFO, (unsigned long)cr) < 0) {
		perror("Could not get connection info");
		free(cr);
		hci_close_dev(dd);
		return BLUE_ERROR;
	}

	if (hci_read_rssi(dd, htobs(cr->conn_info->handle), &rssi, 1000) < 0) {
		perror("Could not read RSSI");
		free(cr);
		hci_close_dev(dd);
		return BLUE_ERROR;
	}

	free(cr);
	hci_close_dev(dd);
	
	return rssi;
}

int blue_lq(char *address) {
	struct hci_conn_info_req *cr;
	uint8_t lq;
	int dd, dev_id;

	bdaddr_t bdaddr;
	str2ba(address, &bdaddr);

	dev_id = hci_for_each_dev(HCI_UP, findConn, (long)&bdaddr);
	if (dev_id < 0) {
		if (error) {
			return BLUE_ERROR;
		}
		else {
			return BLUE_NOT_CONNECTED;
		}
	}

	dd = hci_open_dev(dev_id);
	if (dd < 0) {
		perror("Could not open HCI device");
		return BLUE_ERROR;
	}

	if ((cr = malloc(sizeof(struct hci_conn_info_req) + sizeof(struct hci_conn_info))) == NULL) {
		perror("malloc");
		hci_close_dev(dd);
		return BLUE_ERROR;
	}

	bacpy(&cr->bdaddr, &bdaddr);
	cr->type = ACL_LINK;
	if (ioctl(dd, HCIGETCONNINFO, (unsigned long)cr) < 0) {
		perror("Could not get connection info");
		free(cr);
		hci_close_dev(dd);
		return BLUE_ERROR;
	}
	
	if (hci_read_link_quality(dd, htobs(cr->conn_info->handle), &lq, 1000) < 0) {
		perror("Could not read link quality");
		free(cr);
		hci_close_dev(dd);
		return BLUE_ERROR;
	}

	free(cr);
	hci_close_dev(dd);
	
	return lq;
}
int btsock=0, btconn;
int blue_connect(char *btmac) {

#ifdef DEBUG
	fprintf(stderr, "trying to connect to %s\n", btmac);
#endif
	struct sockaddr_rc addr = { 0 };
	// allocate a socket
	btsock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

	// set the connection parameters (who to connect to)
	addr.rc_family = AF_BLUETOOTH;
	addr.rc_channel = (uint8_t) 1;
	str2ba(btmac, &addr.rc_bdaddr );

	// connect to server
	btconn = connect(btsock, (struct sockaddr *)&addr, sizeof(addr));
	return btconn;
}
int blue_disconnect(char *btmac) {
	close(btconn);
	close(btsock);
}
