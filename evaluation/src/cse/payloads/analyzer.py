#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Payload analysis functions.
"""

__maintainer__ = "Raphael Hiesgen"
__email__ = "raphael.hiesgen@haw-hamburg.de"
__copyright__ = "Copyright 2018-2021"

from collections import defaultdict
from urllib.parse import unquote_plus, urlparse

# -- constants ----------------------------------------------------------------

ascii_min = 33
ascii_max = 126

other_acsii = [
    0,  # nul
    9,  # horizontal tab
    10,  # new line
    13,  # carriage return
    32,  # space
]


class PayloadAnalyzer:
    def __init__(self):
       pass 

    def try_decode_hex(self, pl):
        try:
            res = bytearray.fromhex(pl).decode()
            return res, True
        except:
            pass
        return pl, False

    # def estimate_decodability(self, pl, length=15):
    #     in_ascii_range = 0
    #     not_in_ascii_range = 0
    #     range_end = min(len(pl), length * 2)
    #     for i in range(0, range_end, 2):
    #         lhs = pl[i]
    #         rhs = pl[i + 1]
    #         decoded = int(lhs + rhs, 16)
    #         # print(f'decoded {lhs + rhs} to {decoded}')
    #         if (ascii_min <= decoded <= ascii_max) or (decoded in other_acsii):
    #             in_ascii_range += 1
    #         else:
    #             not_in_ascii_range += 1
    #     # Check if more than 2/3 of pl are ASCII (len of pl is already twice the
    #     # characters).
    #     # threshold = range_end / 3
    #     # print(f'{in_ascii_range} / {threshold} ({not_in_ascii_range})')
    #     # if in_ascii_range >= threshold:
    #     if in_ascii_range >= not_in_ascii_range:
    #         return True
    #     else:
    #         return False


    # def manual_decode(self, pl):
    #     string = ""
    #     for i in range(0, len(pl), 2):
    #         lhs = pl[i]
    #         rhs = pl[i + 1]
    #         decoded = int(lhs + rhs, 16)
    #         string += chr(decoded)
    #     return string


    def decode_hex(self, pl):
        # should_be_decodable = estimate_decodability(pl)
        try:
            decoded = bytearray.fromhex(pl).decode()
            # if not should_be_decodable:
            # print(
            # f'looks like I was wrong (is decodedable): "{pl}" --> "{decoded}"')
            return decoded
        except Exception:
            # if should_be_decodable:
            # print(f'looks like I was wrong (is NOT decodedable): {pl}')
            # print(f' > {manual_decode(pl)}')
            return None


    def clean_pl(self, pl):
        pl = pl.replace(";", " ")
        pl = pl.replace("`", " ")
        pl = pl.replace("+", " ")
        pl = pl.replace("${IFS}", " ")
        pl = pl.replace("\r", " ")
        pl = pl.replace("\n", " ")
        pl = pl.replace("\t", " ")
        pl = pl.replace("'", " ")
        pl = pl.replace("(", " ")
        pl = pl.replace("\\", "")
        pl = pl.replace("http:// ", "http://")
        pl = pl.replace("  ", " ")
        pl = pl.replace("&", " ")
        return pl


    def extract(self, ascii_pl, call):
        try:
            pl = ascii_pl[ascii_pl.find(call) :]
            parts = pl.split(" ")
            idx = 0
            args = []
            while parts[idx + 1].startswith("-"):
                args.append(parts[idx + 1])
                args.append(parts[idx + 2])
                idx += 2
            url = parts[idx + 1]
            if url in ["|", ";"]:
                url = ""
            if len(args) > 0:
                url = " ".join(args) + " " + url
            else:
                pass
            return url
        except IndexError:
            return ""


    def is_url(self, x):
        try:
            result = urlparse(x)
            return all([result.scheme, result.netloc, result.path])
        except Exception:
            return False


    def parse_wget_args(self, args):
        arguments = args.strip().split(" ")
        buildingblocks = defaultdict(lambda: "")
        # Get data from options (busybox wget)
        count = len(arguments)
        idx = 0
        while idx < count:
            current = arguments[idx].strip()
            # print(f"[WGET] next arugment: '{current}'")
            idx += 1
            if not current.startswith("-"):
                # Should be the host, other options have an argument starting
                # with '-'.
                # print(f"[WGET] host is '{current}'")
                if "host" not in buildingblocks:
                    buildingblocks["host"] = current
                else:
                    existing = buildingblocks["host"]
                    if current.startswith("http://") and not existing.startswith("http://"):
                        buildingblocks["host"] = current
                    elif self.is_url(current) and not self.is_url(existing):
                        buildingblocks["host"] = current
            else:
                if current == "-g":
                    # This is a download.
                    # print("[WGET] is a download")
                    pass
                elif current == "-l":
                    if idx < count:
                        buildingblocks["local_path"] = arguments[idx].strip()
                        # print(f"[WGET] local path: '{buildingblocks['local_path']}'")
                        idx += 1
                elif current == "-r":
                    if idx < count:
                        buildingblocks["remote_path"] = arguments[idx].strip()
                        # print(f"[WGET] remote path: '{buildingblocks['remote_path']}'")
                        idx += 1
                else:
                    print(f'unknown wget option: "{current}"')
                    pass
        # Check if we have enough to build the URL
        if buildingblocks["host"] == "":
            print(f"could not build URL from {args}")
            return None
        # Build URL from host and remote path
        url = "http://"
        url += buildingblocks["host"]
        rpath = buildingblocks["remote_path"]
        if not url.endswith("/") and not rpath.startswith("/"):
            url += "/"
        url += rpath
        return url


    def parse_curl_args(self, args):
        parts = args.split(" ")
        buildingblocks = defaultdict(lambda: "")
        for idx in range(0, len(parts), 2):
            # print(f'checking index {idx}')
            opt = parts[idx]
            if not opt.startswith("-"):
                # print(f' no longer an argument, skipping rest: {parts[idx +1:]}')
                continue
            arg = parts[idx + 1]
            if opt == "-O":
                if "host" not in buildingblocks:
                    buildingblocks["host"] = arg
                else:
                    existing_arg = buildingblocks["host"]
                    if arg.startswith("http://") and not existing_arg.startswith("http://"):
                        buildingblocks["host"] = arg
                    elif self.is_url(arg) and not self.is_url(existing_arg):
                        buildingblocks["host"] = arg
            elif opt == "-A" or opt == "--user-agent":
                # Currently ignored, but could be important for downloads?
                buildingblocks["useragent"] = arg
            # TODO: Check other long options.
            elif len(opt) > 2 and not opt.startswith("--"):
                # This should parse options like -sSL or similar.
                options = opt[1:]
                host_is_next = True
                for c in options:
                    if c in ["f", "s", "S", "L"]:
                        # f: fail silently, don't care
                        # s: silent mode, don't care
                        # S: Show error if it fails, don't care
                        # L: location, i.e., "If  the  server  reports that the
                        #   requested page has moved to a different location [...]
                        #   this option will make curl redo the request on the new
                        #   place."
                        continue
                    else:
                        print(f"uknown curl short opt: {c}")
                if host_is_next:
                    buildingblocks["host"] = arg
            else:
                print(f'unknown option: "{opt}"')
                continue
        # Check if we have enough to build the URL
        if (
            "host" not in buildingblocks
            or buildingblocks["host"] is None
            or buildingblocks["host"] == ""
        ):
            print(f"could not build URL from {args}")
            return None
        # Build URL from host and remote path
        url = buildingblocks["host"]
        if not url.startswith("http"):
            url = "http://" + url
        if "remote_path" in buildingblocks:
            rpath = buildingblocks["remote_path"]
            if not url.endswith("/") and not rpath.startswith("/"):
                url += "/"
            url += rpath
        return url


    def extract_name_from_url(self, url):
        name = None
        if "/" in url:
            parts = url.split("/")
            name = parts[-1]
        else:
            print(f"[NAME] encountered URL without name: {url}")
            name = url  # TODO: add a fake name
        return name


    def extract_url_for_tool(self, ascii_pl, tool, args_parser_fun):
        url = self.extract(ascii_pl, tool)
        if url == "http":
            url = "MISSING"
            print(f"[ERR] failed to extract URL from '{ascii_pl}'")
            return None
        if url == "":
            print(f"[PARSE] failed to extract URL from '{ascii_pl}'")
            return None

        if url.startswith("-"):
            url_parsed_from_args = args_parser_fun(url)
            if url is None:
                print(f'failed to parse "{url}')
                return None
            # print(f"[PARSE] got '{url_parsed_from_args}' from '{url}' for '{tool}'")
            url = url_parsed_from_args

        if url is None or url == "":
            print(f"[PARSE] failed find URL in '{ascii_pl}'")
            return None

        if not url.startswith("http://"):
            url = "http://" + url
        server = urlparse(url).netloc

        port = 80
        if ":" in server:
            parts = server.split(":")
            if len(parts) == 2:
                server = parts[0]
                port = parts[1]
        # print(f"{tool}|{url}|{server}|{port}")
        # build event
        res = {
                "tool": tool,
                "url": url,
                "server": server,
                "port": port,
                "name": self.extract_name_from_url(url),
        }
        return res

    # Returns `None` or dictionary with the keys:
    #  "tool"
    #  "url"
    #  "server"
    #  "port"
    #  "name"
    def extract_url(self, ascii_pl):
        pl = self.clean_pl(unquote_plus(ascii_pl))
        # Check wget
        if "wget" in pl:
            elem = None
            elem = self.extract_url_for_tool(pl, "wget", self.parse_wget_args)
            if elem is not None:
                return elem
        # Check curl
        if "curl" in pl:
            elem = None
            if "User-Agent: curl" not in pl:
                elem = self.extract_url_for_tool(pl, "curl", self.parse_curl_args)
            else:
                duplicate = pl.replace("User-Agent: curl", "")
                if "curl" in duplicate:
                    print(f"[PARSE] found curl in user agent and payload: '{pl}'")
                    elem = self.extract_url_for_tool(duplicate, "curl", self.parse_curl_args)
            if elem is not None:
                return elem
        return None


    def extract_path_for_tool(self, ascii_pl, tool, args_parser_fun):
        url = self.extract(ascii_pl, tool)
        if url == "http":
            url = "MISSING"
            print(f"[ERR] failed to extract URL from '{ascii_pl}'")
            return None
        if url == "":
            print(f"[PARSE] failed to extract URL from '{ascii_pl}'")
            return None

        if url.startswith("-"):
            url_parsed_from_args = args_parser_fun(url)
            if url is None:
                print(f'failed to parse "{url}')
                return None
            # print(f"[PARSE] got '{url_parsed_from_args}' from '{url}' for '{tool}'")
            url = url_parsed_from_args

        if url is None or url == "":
            print(f"[PARSE] failed find URL in '{ascii_pl}'")
            return None

        if not url.startswith("http://"):
            url = "http://" + url
        server = urlparse(url).netloc
        path = urlparse(url).path

        port = 80
        if ":" in server:
            parts = server.split(":")
            if len(parts) == 2:
                server = parts[0]
                port = parts[1]
        # print(f"{tool}|{url}|{server}|{port}")
        # build event
        res = {
            "tool": tool,
            "url": url,
            "server": server,
            "port": port,
            "name": self.extract_name_from_url(url),
            "path": path,
        }
        return res


    # Returns `None` or dictionary with the keys:
    #  "tool"
    #  "url"
    #  "server"
    #  "port"
    #  "name"
    def extract_path(self, ascii_pl):
        pl = self.clean_pl(unquote_plus(ascii_pl))
        # Check wget
        if "wget" in pl:
            elem = None
            elem = self.extract_path_for_tool(pl, "wget", self.parse_wget_args)
            if elem is not None:
                return elem
        # Check curl
        if "curl" in pl:
            elem = None
            if "User-Agent: curl" not in pl:
                elem = self.extract_path_for_tool(pl, "curl", self.parse_curl_args)
            else:
                duplicate = pl.replace("User-Agent: curl", "")
                if "curl" in duplicate:
                    print(f"[PARSE] found curl in user agent and payload: '{pl}'")
                    elem = self.extract_path_for_tool(duplicate, "curl", self.parse_curl_args)
            if elem is not None:
                return elem
        return None

