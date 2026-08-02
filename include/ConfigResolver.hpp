#ifndef CONFIG_RESOLVER_HPP
# define CONFIG_RESOLVER_HPP

# include "Config.hpp"

/*
 * Turn the raw parsed config into the typed runtime config. Filesystem paths
 * given relative are taken from prefix, the directory holding the config file,
 * so a config behaves the same whatever the working directory is. Absolute
 * paths are used as written.
 */
Config resolve(const RawConfig &raw, const std::string &prefix);

#endif
