#include "queue.h"
#include "skel.h"

static int cmp_rtable(const void *a, const void *b) {
	struct route_table_entry entry1 = *(struct route_table_entry *)a;
	struct route_table_entry entry2 = *(struct route_table_entry *)b;
	int result = 0;

	result = ntohl(entry1.prefix & entry1.mask) - ntohl(entry2.prefix & entry2.mask);

	if (result == 0) {
		result = ntohl(entry1.mask) - ntohl(entry2.mask);
	}

	return result;
}

/**
 * @brief
 * 
 * @param rtrable routing table
 * @param rtable_len number of entries in routing table
 * @param dest_ip destination IP
 * @return: struct route_table_entry* A pointer to the best route entry from the given routing table.
 * Uses binary search.
 */
static struct route_table_entry *get_best_route(struct route_table_entry *rtable, int rtable_len, struct in_addr dest_ip)
{
	size_t idx = -1;
	int start = 0;
	int end = rtable_len - 1;

	while (start <= end) {
		int mid = (start + end) / 2;

		if ((dest_ip.s_addr & rtable[mid].mask) == (rtable[mid].prefix & rtable[mid].mask)) {
			idx = mid;
			start = mid + 1;
		} else if (ntohl(dest_ip.s_addr & rtable[mid].mask) < ntohl(rtable[mid].prefix & rtable[mid].mask)) {
			end = mid - 1;
		} else if (ntohl(dest_ip.s_addr & rtable[mid].mask) > ntohl(rtable[mid].prefix & rtable[mid].mask)) {
			start = mid + 1;
		}
	}

	return (idx == -1) ? NULL : &rtable[idx];
}

/**
 * @brief
 *
 * @param arp_table ARP cache
 * @param arp_table_len number of entries in ARP cache
 * @param dest_ip destination IP
 * @return struct arp_entry* A pointer to the correct entry in the ARP cache.
 */
static struct arp_entry *get_arp_entry(struct arp_entry *arp_table, int arp_table_len, struct in_addr dest_ip)
{
	for (size_t i = 0; i < arp_table_len; i++) {
		if (memcmp(&dest_ip, &arp_table[i].ip, sizeof(struct in_addr)) == 0) {
			return &arp_table[i];
		}
	}

	return NULL;
}

/**
 * @brief 
 * BONUS: Computes checksum as described by RFC 1624.
 * @param data IP header 
 */
void fast_checksum(void *data)
{
	struct iphdr *iph = (struct iphdr *) data;
	uint16_t HC_old = iph->check;
	uint16_t m_old = (iph->ttl-- & MASK_UINT16);
	uint16_t m_new = (iph->ttl & MASK_UINT16);
	uint16_t HC_new = ~(~HC_old + ~m_old + m_new) - 1;	// HC' = ~(~HC + ~m + m')
	iph->check = HC_new;
}

int main(int argc, char *argv[])
{
	packet m;
	int rc;

	// Do not modify this line
	init(argc - 2, argv + 2);

	// routing table with unspecified number of entries
	struct route_table_entry *rtable;
	int rtable_len;

	// alloc memory for rtable
	rtable = malloc(sizeof(struct route_table_entry) * MAX_ROUTER_ENTRIES);
	DIE(rtable == NULL, "memory");
	
	// read rtable
	rtable_len = read_rtable(argv[1], rtable);

	// sort rtable by prefix and mask
	qsort(rtable, rtable_len, sizeof(struct route_table_entry), cmp_rtable);
	
	// initiate arp cache
	struct arp_entry *arp_cache;
	int arp_cache_len = 0;

	// alloc memory for arp cache
	arp_cache = malloc(sizeof(struct arp_entry) * 100);
	DIE(arp_cache == NULL, "memory");

	// create message queue
	queue m_queue = queue_create();

	struct iphdr *iph;
	struct in_addr dest_ip;
	struct icmphdr *icmph;
	struct arp_header *arph;

	while (1) {
		rc = get_packet(&m);
		DIE(rc < 0, "get_packet");
		
		struct ether_header *eth = (struct ether_header *) m.payload;
		uint16_t ether_type = ntohs(eth->ether_type);

		if (ether_type == ETHERTYPE_IP) {
			iph = ((void *) eth) + sizeof(struct ether_header);
			icmph = NULL;

			// check icmp header
			if (iph->protocol == IPPROTO_ICMP) {
				icmph = ((void *) eth) + sizeof(struct ether_header) + sizeof(struct iphdr);
				// check the pckg is for the router, then send icmp reply
				if ((icmph->type == ICMP_ECHO) && (iph->daddr == inet_addr(get_interface_ip(m.interface)))) {
					send_icmp(iph->saddr, iph->daddr, eth->ether_dhost, eth->ether_shost, ICMP_ECHOREPLY, ICMP_ECHOREPLY, m.interface, icmph->un.echo.id, icmph->un.echo.sequence);
					continue;
				}
			}

			// check if pckg is corrupt
			if (ip_checksum((void *)iph, sizeof(struct iphdr)) != 0) {
				continue;
			}

			// check if ttl is 1 or 0
			if (iph->ttl == 1 || iph->ttl == 0) {
				// ICMP message - "Time exceeded"
				send_icmp_error(iph->saddr, iph->daddr, eth->ether_dhost, eth->ether_shost, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, m.interface);
				continue;
			}

			// set destination address
			dest_ip.s_addr = iph->daddr;

			struct route_table_entry *route = get_best_route(rtable, rtable_len, dest_ip);
			if (!route) {
				// ICMP message -- "Destination unreachable"
				send_icmp_error(iph->saddr, iph->daddr, eth->ether_dhost, eth->ether_shost, ICMP_DEST_UNREACH, ICMP_NET_UNREACH, m.interface);
				continue;
			}
			// save interface in message
			memcpy(&m.interface, &route->interface, sizeof(int));
			
			struct arp_entry *arp_entry = get_arp_entry(arp_cache, arp_cache_len, dest_ip);
			if (!arp_entry) {
				// make copy of packet and save it in message queue
				packet *m_copy = malloc(sizeof(m));
				memcpy(m_copy, &m, sizeof(m));
				queue_enq(m_queue, m_copy);

				// create arp ethernet header
				struct ether_header *eth_arp = malloc(sizeof(*eth_arp));
				eth_arp->ether_type = htons(ETHERTYPE_ARP);
				get_interface_mac(route->interface, eth_arp->ether_shost);
				memset(eth_arp->ether_dhost, 0xff, ETH_ALEN);
				
				// send arp request
				send_arp(route->next_hop, inet_addr(get_interface_ip(m.interface)), eth_arp, m.interface, ARPOP_REQUEST);
				continue;
			}

			// update ttl and checksum
			fast_checksum(iph);

			// update ethernet header
			memcpy(eth->ether_dhost, arp_entry->mac, ETH_ALEN);
			get_interface_mac(route->interface, eth->ether_shost);

			// send packet
			send_packet(&m);
		} else if (ether_type == ETHERTYPE_ARP) {
			arph = ((void *) eth) + sizeof(struct ether_header);

			if (arph->op == htons(ARPOP_REQUEST)) {
				// create arp ethernet header
				struct ether_header *eth_arp = malloc(sizeof(*eth_arp));
				eth_arp->ether_type = htons(ETHERTYPE_ARP);
				get_interface_mac(m.interface, eth_arp->ether_shost);
				memcpy(eth_arp->ether_dhost, eth->ether_shost, ETH_ALEN);

				// send arp reply
				send_arp(arph->spa, arph->tpa, eth_arp, m.interface, ARPOP_REPLY);
			} else {
				// create new arp entry
				struct arp_entry new_arp_entry;
				new_arp_entry.ip = arph->spa;
				memcpy(new_arp_entry.mac, arph->sha, ETH_ALEN);

				// add arp entry to cache
				arp_cache[arp_cache_len++] = new_arp_entry;

				// create temporary queue
				queue queue_tmp = queue_create();
				packet *m_tmp;
				struct ether_header *eth_tmp;
				struct iphdr *iph_tmp;
				struct in_addr dest_ip_tmp;
				struct route_table_entry *route_tmp;

				// inspect each packet in message queue
				while (!queue_empty(m_queue)) {
					m_tmp = (packet *) queue_deq(m_queue);

					eth_tmp = (struct ether_header *) m_tmp->payload;
					iph_tmp = ((void *) eth_tmp) + sizeof(struct ether_header);

					dest_ip_tmp.s_addr = iph_tmp->daddr;

					route_tmp = get_best_route(rtable, rtable_len, dest_ip_tmp);

					if (route_tmp->next_hop != new_arp_entry.ip) {
						// if it's not the right packet, save it in temporary queue
						queue_enq(queue_tmp, m_tmp);
					} else {
						// update ttl and checksum
						fast_checksum(iph);

						// update ethernet header
						memcpy(eth_tmp->ether_dhost, new_arp_entry.mac, ETH_ALEN);
						get_interface_mac(route_tmp->interface, eth->ether_shost);

						// send packet
						send_packet(m_tmp);
					}
				}

				// move all packets from the temporary queue to the message queue
				while (!queue_empty(queue_tmp)) {
					queue_enq(m_queue, queue_deq(queue_tmp));
				}
			}
		}
	}

	free(rtable);
}
