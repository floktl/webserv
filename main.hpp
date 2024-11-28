/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.hpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/28 10:35:17 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/28 14:34:56 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# ifndef DEBUG
#  define DEBUG 1
# endif

# ifndef CONFIG_OPTS
#  define CONFIG_OPTS "server,listen,server_name,root,index,error_page,location"
# endif

# ifndef LOCATION_OPTS
#  define LOCATION_OPTS "methods,autoindex,default_file,cgi,cgi_param"
# endif
