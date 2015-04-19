#!/usr/bin/python
# -*- coding:UTF-8 -*-

import urllib2

def page(url):
	print '    downing...'
	context = urllib2.urlopen('http://' + url).read()
	f = open('htdocs/index.html', 'w+')
	f.write(context)
	f.close()
	print '    download ok'

