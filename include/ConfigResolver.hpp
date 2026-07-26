#ifndef CONFIG_RESOLVER_HPP
# define CONFIG_RESOLVER_HPP

# include "Config.hpp"

// Turn the raw parsed config into the typed runtime config.
Config resolve(const RawConfig &raw);

#endif
