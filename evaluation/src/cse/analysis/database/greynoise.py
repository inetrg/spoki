#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Query our IPs on greynoise.io.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

import json
import requests


class GreyNoise:
    def __init__(self, apikey):
        self.apikey = apikey

    # -- urls ------------------------------------------------------------------

    community_url = "https://api.greynoise.io/v3/community/"

    base_url = "https://api.greynoise.io/v2/"
    noise_url = base_url + "noise/{}"
    context_url = noise_url.format("context/")
    quick_url = noise_url.format("quick/")
    multi_quick_url = noise_url.format("multi/quick")
    multi_context_url = noise_url.format("multi/context")

    riot_url = base_url + "riot/"

    # -- return codes ----------------------------------------------------------

    return_codes = {
        0x00: "The IP has never been observed scanning the Internet",
        0x01: "The IP has been observed by the GreyNoise sensor network",
        0x02: "The IP has been observed scanning the GreyNoise sensor network, "
        "but has not completed a full connection, meaning this can be spoofed",
        0x03: "The IP is adjacent to another host that has been directly "
        "observed by the GreyNoise sensor network",
        0x04: "Reserved",
        0x05: "This IP is commonly spoofed in Internet-scan activity",
        0x06: "This IP has been observed as noise, but this host belongs to a "
        "cloud provider where IPs can be cycled frequently",
        0x07: "This IP is invalid",
        0x08: "This IP was classified as noise, but has not been observed "
        "engaging in Internet-wide scans or attacks in over 60 days",
    }

    # -- single ip queries -----------------------------------------------------

    def context_query(self, ip):
        url = self.context_url + str(ip)
        headers = {
            "Accept": "application/json",
            "key": self.apikey,
        }
        response = requests.request("GET", url, headers=headers)
        return json.loads(response.text)

    def quick_query(self, ip):
        url = self.quick_url + str(ip)
        headers = {
            "Accept": "application/json",
            "key": self.apikey,
        }
        response = requests.request("GET", url, headers=headers)
        return json.loads(response.text)

    def riot_query(self, ip):
        url = self.riot_url + str(ip)
        headers = {
            "Accept": "application/json",
            "key": self.apikey,
        }
        response = requests.request("GET", url, headers=headers)
        return json.loads(response.text)

    def community_query(self, ip):
        headers = {
            "Accept": "application/json",
            "key": self.apikey,
        }
        response = requests.request("GET", self.community_url, headers=headers)
        return json.loads(response.text)

    # -- multi ip queries ------------------------------------------------------

    def multi_context_query(self, ips):
        payload = {"ips": ips}
        headers = {
            "Accept": "application/json",
            "Content-Type": "application/json",
            "key": self.apikey,
        }
        response = requests.request(
            "POST", self.multi_context_url, json=payload, headers=headers
        )
        return json.loads(response.text)

    def multi_quick_query(self, ips):
        payload = {"ips": ips}
        headers = {
            "Accept": "application/json",
            "Content-Type": "application/json",
            "key": self.apikey,
        }

        response = requests.request(
            "POST", self.multi_quick_url, json=payload, headers=headers
        )
        return json.loads(response.text)

    def multi_quick_query_alt(self, ips):
        assert len(ips) <= 500, "multi_quick_query_alt only supports up to 500 ips"
        payload = {"ips": ips}
        headers = {
            "Accept": "application/json",
            "Content-Type": "application/json",
            "key": self.apikey,
        }
        response = requests.request(
            "GET", self.multi_quick_url, json=payload, headers=headers
        )
        return json.loads(response.text)
