/*
 *
 *  Copyright ( c ) 2011
 *  name : mhogo mchungu
 *  email: mhogomchungu@gmail.com
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  ( at your option ) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MISCFUNCTIONS_H
#define MISCFUNCTIONS_H

#include <QString>
#include <QStringList>

class utility
{
public:
	static QStringList luksEmptySlots( QString volumePath ) ;
	static bool isLuks( QString volumePath ) ;
	static void addToFavorite( QString dev,QString m_point );
	static QStringList readFavorites( void );
	static void removeFavoriteEntry( QString );
	static bool exists( QString );
	static bool canCreateFile( QString );
	static QString resolvePath( QString );
	static QString hashPath( QString p );
	static QString cryptMapperPath( void );
	static void debug( QString );
	static void debug( int );
	static QString mapperPath( QString );
	static QString getUUIDFromPath( QString ) ;
	static bool userIsRoot( void ) ;
	static bool mapperPathExists( QString path ) ;
	static QString mountPath( QString ) ;
	static QString userName( void ) ;
	static void help( QString app ) ;
	static QString shareMountPointToolTip( void ) ;
	static QString shareMountPointToolTip( QString ) ;
	static QString sharedMountPointPath( QString ) ;
	static bool pathPointsToAFile( QString ) ;
	static QString localizationLanguage( QString ) ;
	static QString localizationLanguagePath( QString ) ;
	static void setLocalizationLanguage( QString,QString ) ;
	static QString walletName( void ) ;
	static QString defaultKDEWalletName( void ) ;
	static QString applicationName( void ) ;
	static bool pathIsReadable( QString ) ;
};

#endif // MISCFUNCTIONS_H
