#RSSDCC-bot
RSSDCC-bot is an IRC bot that will parse and aggregate RSS feeds for TV series releases, and then download them from XDCC bots on IRC.

The purpose of this is so I can download TV series at a decent speed on a connection that throttles P2P, automatically.

RSSDCC-bot currently only works on GNU/Linux. (Maybe OSX, but I haven't tried)

It's designed with using the RSS feeds of Anime episodes from [Nyaa](www.nyaa.se) and downloading the from the XDCC bots on [#news on Rizon](irc://irc.rizon.net/#news) in mind, but it would probably work on other sites/channels as well.

Compile using `make`.

#Feeds
Place a file for each RSS feed inside the `feeds` directory.

    # Example RSS Feed
    # The host site for the RSS feed
    host=www.nyaa.se
    # The page resource for the RSS feed (note: no '/')
    link=?page=rss&user=73859&term=Rail+wars!
    # The name of the bot on the IRC channel this series will be downloaded from
    bot=[intel]Haruhichan

    # Include a have= for every file you already have, and don't want downloaded
    # More files will be added to this list as you download Them
    have=[FFF] Rail Wars! - 01v2 [C4EF87C0].mkv

Note: Be VERY specific with the RSS feed you provide, otherwise you'll probably get some other crap you don't want to download making it into the feed.

The config file for the IRC channel is `rssdcc.conf`.

#TODO
- Running as a daemon
- Multiple simultaneous downloads
- DCC resume
- More configuration options
- Better error checking
- Better logging
- v2 checking
