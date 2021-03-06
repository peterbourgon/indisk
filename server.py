#!/usr/bin/env python2.7

from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
import indisk

mock_results = """{
	"hits": 123,
	"top": [
	  { "article": "Apple", "weight": 12 },
	  { "article": "Beta", "weight": 7 },
	  { "article": "List of Categories", "weight": 2 },
	  { "article": "Phineas Gage", "weight": 1 },
	  { "article": "Walnuts", "weight": 1 },
	  { "article": "Telephone", "weight": 1 },
	  { "article": "Arlanda", "weight": 1 },
	  { "article": "Charlie Murphy", "weight": 1 },
	  { "article": "Deliberate", "weight": 1 },
	  { "article": "Jury", "weight": 1 }
	]
}"""


class MockHandler(BaseHTTPRequestHandler):
	def do_GET(self):
		tokens = self.path.split("/")[1:]
		if len(tokens) == 1 and tokens[0] == "":
			tokens[0] = "index.html"
		if len(tokens) == 2 and tokens[0] == "query":
			self.send_response(200)
			self.send_header("Content-type", "application/json")
			self.end_headers()
			results = indisk.search(tokens[1].lower())
			self.wfile.write(results)
		else:
			try:
				filename = "/".join(tokens)
				f = open(filename)
				self.send_response(200)
				self.send_header("Content-type", "text/html")
				self.end_headers()
				self.wfile.write(f.read())
				f.close()
			except IOError:
				self.send_error(500, "Error loading file %s" % filename)

def main(args):
	try:
		print "parsing %d index files" % len(args)
		count = indisk.init(args)
		print "searching %d index files" % count
		server = HTTPServer(("", 8080), MockHandler)
		server.serve_forever()
	except KeyboardInterrupt:
		print "shutdown"
		server.socket.close()

import sys
if __name__ == "__main__":
	if len(sys.argv) < 2:
		print "usage: %s <idx> [<idx> ...]" % sys.argv[0]
		sys.exit(1)
	main(sys.argv[1:])

