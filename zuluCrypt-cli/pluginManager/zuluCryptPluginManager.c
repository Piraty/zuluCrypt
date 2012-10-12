/*
 * 
 *  Copyright (c) 2012
 *  name : mhogo mchungu 
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <blkid/blkid.h>
#include <sys/syscall.h>
#include <fcntl.h>

#include "libzuluCryptPluginManager.h"
#include "../process/process.h"
#include "../socket/socket.h"
#include "../string/String.h"
#include "../string/StringManage.h"
#include "../constants.h"
#include "../bin/includes.h"

#ifdef __STDC__
int syscall(int number, ...) ;
#endif
/*
 * below header file is created at config time.
 */
#include "plugin_path.h"
#include <stdio.h>

static socket_t zuluCryptSocketAccept( socket_t server ) 
{
	int i ;
	int fd ;
	
	socket_t client = SocketVoid ;
	
	if( server == SocketVoid )
		return SocketVoid ;
	
	fd = SocketFileDescriptor( server ) ;
	
	fcntl( fd,F_SETFL,O_NONBLOCK );
	
	for( i = 0 ; i < 30 ; i++ ){
		client = SocketAccept( server ) ;
		if( client != SocketVoid ){
			break ;	
		}else{
			sleep( 1 ) ;
		}
	}	
	
	return client ;
}

size_t zuluCryptGetKeyFromSocket( const char * sockpath,string_t * key,uid_t uid )
{	
	size_t dataLength = 0 ;
	char * buffer ;

	socket_t client ;
	
	socket_t server = SocketLocal( sockpath ) ;
	
	SocketBind( server ) ;
	
	chown( sockpath,uid,uid ) ;
	chmod( sockpath,S_IRWXU | S_IRWXG | S_IRWXO ) ;
	
	SocketListen( server ) ;
	
	client = zuluCryptSocketAccept( server ) ;
	
	dataLength = SocketGetData( client,&buffer,INTMAXKEYZISE ) ;
	
	SocketClose( server ) ;
	SocketClose( client ) ;
	
	SocketDelete( &server ) ;
	SocketDelete( &client ) ;
	
	*key = StringInheritWithSize( &buffer,dataLength ) ;
	
	return dataLength ;
}

void * zuluCryptPluginManagerOpenConnection( const char * sockpath )
{
	int i ;	
	socket_t client = SocketLocal( sockpath ) ;
		
	for( i = 0 ;  ; i++ ){
		if( SocketConnect( client ) == 0 )
			return ( void * ) client ;
		else if( i == 20 ){
			SocketDelete( &client ) ;
			break ;
		}else
			sleep( 1 ) ;
	}
	
	return NULL ;
}

ssize_t zuluCryptPluginManagerSendKey( void * client,const char * key,size_t length )
{
	return client == NULL ? -1 : SocketSendData( ( socket_t )client,key,length ) ;
}

void zuluCryptPluginManagerCloseConnection( void * p )
{	
	if( p != NULL ){
		socket_t client = ( socket_t ) p;
		SocketClose( client ) ;
		SocketDelete( &client ) ;
	}
}

static string_t zuluCryptGetDeviceUUID( const char * device )
{
	string_t p ;
	blkid_probe blkid ;
	const char * uuid ;
	
	blkid = blkid_new_probe_from_filename( device ) ;
	blkid_do_probe( blkid );
	
	if( blkid_probe_lookup_value( blkid,"UUID",&uuid,NULL ) == 0 ){
		p = String( uuid ) ;
	}else{
		p = String( "Nil" ) ;
	}
	
	blkid_free_probe( blkid );
	
	return p ;
}

string_t zuluCryptPluginManagerGetKeyFromModule( const char * device,const char * name,uid_t uid,const char * argv )
{	
	socket_t server ;
	socket_t client ;
	
	char * buffer ;

	process_t p ;
	
	int i ;
	const char * sockpath ;
	
	string_t key   = StringVoid ;
	string_t plugin_path = StringVoid ;
	string_t path  = StringVoid ;
	string_t id    = StringVoid ;
	string_t uuid  = StringVoid ;

	struct passwd * pass = getpwuid( uid ) ;
		
	if( pass == NULL )
		return NULL ;	
	
	if( strrchr( name,'/' ) == NULL ){
		/*
		 * ZULUCRYPTpluginPath is set at config time at it equals $prefix/lib(64)/zuluCrypt/
		 */
		plugin_path = String( ZULUCRYPTpluginPath ) ;
		StringAppend( plugin_path,name ) ;
	}else{
		/*
		 * module has a backslash, assume its path to where a module is located
		 */
		plugin_path = String( name ) ;
	}
	
	path = String( pass->pw_dir ) ;
	sockpath = StringAppend( path,"/.zuluCrypt-socket/" ) ;
	
	mkdir( sockpath,S_IRWXU | S_IRWXG | S_IRWXO ) ;
	chown( sockpath,uid,uid ) ;
	chmod( sockpath,S_IRWXU ) ;
	
	id = StringIntToString( syscall( SYS_gettid ) ) ;
	
	sockpath = StringAppendString( path,id ) ;
	
	uuid = zuluCryptGetDeviceUUID( device ) ;

	p = Process( StringContent( plugin_path ) ) ;

	ProcessSetOptionUser( p,uid ) ;
	ProcessSetArgumentList( p,device,StringContent( uuid ),sockpath,CHARMAXKEYZISE,argv,'\0' ) ;
	ProcessStart( p ) ;
	
	server = SocketLocal( sockpath ) ;
	
	SocketBind( server ) ;
	
	chown( sockpath,uid,uid ) ;
	chmod( sockpath,S_IRWXU | S_IRWXG | S_IRWXO ) ;
	
	SocketListen( server ) ;
	
	client = zuluCryptSocketAccept( server ) ;
	
	SocketClose( server ) ;
	SocketDelete( &server ) ;
	
	i = SocketGetData( client,&buffer,INTMAXKEYZISE ) ;
	
	SocketClose( client ) ;
	SocketDelete( &client ) ;
	
	key = StringInheritWithSize( &buffer,i ) ;
	
	/*
	 * for reasons currently unknown to me,the gpg plugin doesnt always exit,it hangs tying up cpu circles.
	 * send it a sigterm after it is done sending its key to make sure it exits.
	 */
	if( StringEqual( plugin_path,ZULUCRYPTpluginPath"gpg" ) )
		ProcessTerminate( p ) ;
	
	ProcessDelete( &p ) ;
	
	StringMultipleDelete( &plugin_path,&uuid,&id,&path,'\0' ) ;      
	
	return key ;
}
