call converter\IncludeConverter.exe tomic\3rd-party\mioc -i include -s include src
call converter\IncludeConverter.exe tomic\3rd-party\twio -i include -s include src
call converter\IncludeConverter.exe tomic -i include -s include src
call converter\IncludeConverter.exe tomic -i 3rd-party\mioc\include 3rd-party\twio\include -s include src
call converter\IncludeConverter.exe -i tomic\include -s src