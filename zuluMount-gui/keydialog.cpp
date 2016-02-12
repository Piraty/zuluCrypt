/*
 *
 *  Copyright (c) 2012-2015
 *  name : Francis Banyikwa
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

#include "deviceoffset.h"

#include "keydialog.h"
#include "ui_keydialog.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QDir>
#include <QTableWidget>
#include <QDebug>
#include <QFile>

#include "bin_path.h"
#include "../zuluCrypt-gui/dialogmsg.h"
#include "../zuluCrypt-gui/task.h"
#include "../zuluCrypt-cli/constants.h"
#include "plugin_path.h"
#include "../zuluCrypt-gui/utility.h"
#include "../zuluCrypt-gui/lxqt_wallet/frontend/lxqt_wallet.h"
#include "mountoptions.h"
#include "../zuluCrypt-gui/tcrypt.h"
#include "zulumounttask.h"
#include "../zuluCrypt-gui/task.h"
#include "zulumounttask.h"
#include "veracrypt_support.h"
#include "truecrypt_support.h"
#include "veracryptpimdialog.h"

#define KWALLET         "kde wallet"
#define INTERNAL_WALLET "internal wallet"
#define GNOME_WALLET    "gnome wallet"

/*
 * this ugly global variable is defined in zulucrypt.cpp to prevent multiple prompts when opening multiple volumes
 */
static QString _internalPassWord ;

keyDialog::keyDialog( QWidget * parent,QTableWidget * table,const volumeEntryProperties& e,std::function< void() > p,std::function< void( const QString& ) > q ) :
	QDialog( parent ),m_ui( new Ui::keyDialog ),m_cancel( std::move( p ) ),m_success( std::move( q ) )
{
	m_ui->setupUi( this ) ;
	m_ui->checkBoxShareMountPoint->setToolTip( utility::shareMountPointToolTip() ) ;
	m_table = table ;
	m_path = e.volumeName() ;
	m_working = false ;
	m_volumeIsEncFs = e.fileSystem() == "encfs" ;

	decltype( tr( "" ) ) msg ;

	if( e.fileSystem() == "crypto_LUKS" ){

		msg = tr( "Mount A LUKS volume in \"%1\"").arg( m_path ) ;
	}else{
		msg = tr( "Mount An Encrypted Volume In \"%1\"").arg( m_path ) ;
	}

	this->setWindowTitle( msg ) ;

	m_ui->lineEditMountPoint->setText( m_path ) ;
	m_ui->pbOpenMountPoint->setIcon( QIcon( ":/folder.png" ) ) ;

	m_menu = new QMenu( this ) ;

	connect( m_menu,SIGNAL( triggered( QAction * ) ),this,SLOT( pbPluginEntryClicked( QAction * ) ) ) ;

	this->setFixedSize( this->size() ) ;
	this->setWindowFlags( Qt::Window | Qt::Dialog ) ;
	this->setFont( parent->font() ) ;

	m_ui->lineEditKey->setFocus() ;

	m_ui->checkBoxOpenReadOnly->setChecked( utility::getOpenVolumeReadOnlyOption( "zuluMount-gui" ) ) ;

	m_ui->pbkeyOption->setEnabled( false ) ;

	m_ui->lineEditKey->setEchoMode( QLineEdit::Password ) ;

	connect( m_ui->pbOptions,SIGNAL( clicked() ),this,SLOT( pbOptions() ) ) ;
	connect( m_ui->pbCancel,SIGNAL( clicked() ),this,SLOT( pbCancel() ) ) ;
	connect( m_ui->pbOpen,SIGNAL( clicked() ),this,SLOT( pbOpen() ) ) ;
	connect( m_ui->pbkeyOption,SIGNAL( clicked() ),this,SLOT( pbkeyOption() ) ) ;
	connect( m_ui->pbOpenMountPoint,SIGNAL( clicked() ),this,SLOT( pbMountPointPath() ) ) ;
	connect( m_ui->checkBoxOpenReadOnly,SIGNAL( stateChanged( int ) ),this,SLOT( cbMountReadOnlyStateChanged( int ) ) ) ;
	connect( m_ui->cbKeyType,SIGNAL( currentIndexChanged( int ) ),this,SLOT( cbActicated( int ) ) ) ;

	m_ui->pbOpenMountPoint->setVisible( false ) ;

	const auto& m = e.mountPoint() ;

	if( m.isEmpty() || m == "Nil" ){

		m_point = utility::mountPathPostFix( m_path.split( "/" ).last() ) ;
	}else{
		m_point = utility::mountPathPostFix( m ) ;
	}

	m_ui->lineEditMountPoint->setText( m_point ) ;

	auto ac = new QAction( this ) ;
	QKeySequence s( Qt::CTRL + Qt::Key_F ) ;
	ac->setShortcut( s ) ;
	connect( ac,SIGNAL( triggered() ),this,SLOT( showOffSetWindowOption() ) ) ;
	this->addAction( ac ) ;

	m_menu_1 = new QMenu( this ) ;

	m_menu_1->setFont( this->font() ) ;

	auto _add_action = [ & ]( const QString& e ){

		ac = m_menu_1->addAction( e ) ;
		ac ->setEnabled( !m_volumeIsEncFs ) ;
	} ;

	_add_action( tr( "Set File System Options" ) ) ;
	_add_action( tr( "Set Volume Offset" ) ) ;
	_add_action( tr( "Set Volume As VeraCrypt Volume" ) ) ;
	_add_action( tr( "Set VeraCrypt PIM value" ) ) ;

	m_ui->cbKeyType->addItem( tr( "Key" ) ) ;
	m_ui->cbKeyType->addItem( tr( "KeyFile" ) ) ;
	m_ui->cbKeyType->addItem( tr( "Key+KeyFile" ) ) ;
	m_ui->cbKeyType->addItem( tr( "Plugin" ) ) ;

	if( m_volumeIsEncFs ){

		m_ui->checkBoxShareMountPoint->setEnabled( false ) ;
	}else{
		m_ui->cbKeyType->addItem( tr( "TrueCrypt/VeraCrypt Keys" ) ) ;
	}

	connect( m_menu_1,SIGNAL( triggered( QAction * ) ),this,SLOT( doAction( QAction * ) ) ) ;

	m_veraCryptWarning.setWarningLabel( m_ui->veraCryptWarning ) ;

	this->installEventFilter( this ) ;
}

bool keyDialog::eventFilter( QObject * watched,QEvent * event )
{
	return utility::eventFilter( this,watched,event,[ this ](){ this->pbCancel() ; } ) ;
}

void keyDialog::tcryptGui()
{
	this->disableAll() ;
	m_ui->lineEditKey->setText( QString() ) ;

	tcrypt::instance( this,false,[ this ]( const QString& key,const QStringList& keyFiles ){

		m_key = key ;
		m_keyFiles = keyFiles ;

		this->openVolume() ;

		m_ui->cbKeyType->setCurrentIndex( keyDialog::Key ) ;
		m_ui->lineEditKey->setText( QString() ) ;
		m_ui->lineEditKey->setEnabled( false ) ;

	},[ this ](){

		m_key.clear() ;
		m_keyFiles.clear() ;
		m_ui->cbKeyType->setCurrentIndex( keyDialog::Key ) ;
		m_ui->lineEditKey->setText( QString() ) ;
		this->enableAll() ;
	} ) ;
}

void keyDialog::pbOptions()
{
	m_menu_1->exec( QCursor::pos() ) ;
}

void keyDialog::showOffSetWindowOption()
{
	deviceOffset::instance( this,false,[ this ]( const QString& e,const QString& f ){

		Q_UNUSED( f ) ;
		m_deviceOffSet = QString( " -o %1" ).arg( e ) ;
	} ) ;
}

void keyDialog::showFileSystemOptionWindow()
{
	mountOptions::instance( &m_options,this ) ;
}

void keyDialog::doAction( QAction * ac )
{
	auto e = ac->text() ;

	e.remove( "&" ) ;

	if( e == tr( "Set File System Options" ) ){

		this->showFileSystemOptionWindow() ;

	}else if( e == tr( "Set Volume Offset" ) ){

		this->showOffSetWindowOption() ;

	}else if( e == tr( "Set Volume As VeraCrypt Volume" ) ){

		m_veraCryptVolume = true ;

	}else if( e == tr( "Set VeraCrypt PIM value" ) ){

		VeraCryptPIMDialog::instance( this,[ this ]( int e ){

			m_veraCryptPIMValue = e ;
			m_veraCryptVolume = e > 0 ;
		} ) ;
	}
}

void keyDialog::cbMountReadOnlyStateChanged( int state )
{
	m_ui->checkBoxOpenReadOnly->setEnabled( false ) ;
	m_ui->checkBoxOpenReadOnly->setChecked( utility::setOpenVolumeReadOnly( this,state == Qt::Checked,QString( "zuluMount-gui" ) ) ) ;
	m_ui->checkBoxOpenReadOnly->setEnabled( true ) ;

	if( m_ui->lineEditKey->text().isEmpty() ){

		m_ui->lineEditKey->setFocus() ;

	}else if( m_ui->lineEditMountPoint->text().isEmpty() ){

		m_ui->lineEditMountPoint->setFocus() ;
	}else{
		m_ui->pbOpen->setFocus() ;
	}
}

void keyDialog::pbMountPointPath()
{
	auto msg = tr( "Select A Folder To Create A Mount Point In" ) ;
	auto Z = QFileDialog::getExistingDirectory( this,msg,utility::homePath(),QFileDialog::ShowDirsOnly ) ;

	if( !Z.isEmpty() ){

		Z = Z + "/" + m_ui->lineEditMountPoint->text().split( "/" ).last() ;
		m_ui->lineEditMountPoint->setText( Z ) ;
	}
}

void keyDialog::enableAll()
{
	m_ui->pbOptions->setEnabled( true ) ;
	m_ui->label_2->setEnabled( true ) ;
	m_ui->lineEditMountPoint->setEnabled( true ) ;
	m_ui->pbOpenMountPoint->setEnabled( true ) ;
	m_ui->pbCancel->setEnabled( true ) ;
	m_ui->pbOpen->setEnabled( true ) ;
	m_ui->label->setEnabled( true ) ;
	m_ui->cbKeyType->setEnabled( true ) ;

	m_ui->lineEditKey->setEnabled( m_ui->cbKeyType->currentIndex() == keyDialog::Key ) ;

	m_ui->pbkeyOption->setEnabled( true ) ;
	m_ui->checkBoxOpenReadOnly->setEnabled( true ) ;

	if( !m_volumeIsEncFs ){

		m_ui->checkBoxShareMountPoint->setEnabled( true ) ;
	}
}

void keyDialog::disableAll()
{
	m_ui->cbKeyType->setEnabled( false ) ;
	m_ui->pbOptions->setEnabled( false ) ;
	m_ui->pbkeyOption->setEnabled( false ) ;
	m_ui->label_2->setEnabled( false ) ;
	m_ui->lineEditMountPoint->setEnabled( false ) ;
	m_ui->pbOpenMountPoint->setEnabled( false ) ;
	m_ui->lineEditKey->setEnabled( false ) ;
	m_ui->pbCancel->setEnabled( false ) ;
	m_ui->pbOpen->setEnabled( false ) ;
	m_ui->label->setEnabled( false ) ;
	m_ui->checkBoxOpenReadOnly->setEnabled( false ) ;
	m_ui->checkBoxShareMountPoint->setEnabled( false ) ;
}

void keyDialog::KeyFile()
{
	if( m_ui->cbKeyType->currentIndex() == keyDialog::keyfile ){

		auto msg = tr( "Select A File To Be Used As A Keyfile" ) ;
		auto Z = QFileDialog::getOpenFileName( this,msg,utility::homePath() ) ;

		if( !Z.isEmpty() ){

			m_ui->lineEditKey->setText( Z ) ;
		}
	}
}

void keyDialog::pbkeyOption()
{
	auto keyType = m_ui->cbKeyType->currentIndex() ;

	if( keyType == keyDialog::plugin ){

		this->Plugin() ;

	}else if( keyType == keyDialog::keyfile ){

		this->KeyFile() ;
	}
}

void keyDialog::Plugin()
{
	utility::createPlugInMenu( m_menu,tr( INTERNAL_WALLET ),
				   tr( GNOME_WALLET ),tr( KWALLET ),!m_volumeIsEncFs ) ;

	m_menu->setFont( this->font() ) ;

	m_menu->addSeparator() ;

	m_menu->addAction( tr( "Cancel" ) ) ;

	m_menu->exec( QCursor::pos() ) ;
}

void keyDialog::pbPluginEntryClicked( QAction * e )
{
	auto r = e->text() ;

	r.remove( "&" ) ;

	if( r != tr( "Cancel" ) ){

		m_ui->lineEditKey->setText( r ) ;
	}
}

void keyDialog::closeEvent( QCloseEvent * e )
{
	e->ignore() ;
	this->pbCancel() ;
}

void keyDialog::pbOpen()
{
	this->disableAll() ;

	if( m_ui->cbKeyType->currentIndex() == keyDialog::plugin ){

		utility::wallet w ;

		auto wallet = m_ui->lineEditKey->text() ;

		if( wallet == tr( KWALLET ) ){

			w = utility::getKeyFromWallet( LxQt::Wallet::kwalletBackEnd,m_path ) ;

		}else if( wallet == tr( INTERNAL_WALLET ) ){

			w = utility::getKeyFromWallet( LxQt::Wallet::internalBackEnd,m_path,_internalPassWord ) ;

			if( w.notConfigured ){

				DialogMsg msg( this ) ;
				msg.ShowUIOK( tr( "ERROR!" ),tr( "Internal wallet is not configured" ) ) ;
				return this->enableAll() ;
			}else{
				_internalPassWord = w.password ;
			}

		}else if( wallet == tr( GNOME_WALLET ) ){

			w = utility::getKeyFromWallet( LxQt::Wallet::secretServiceBackEnd,m_path ) ;
		}else{
			return this->openVolume() ;
		}

		if( w.opened ){

			if( w.key.isEmpty() ){

				DialogMsg msg( this ) ;

				msg.ShowUIOK( tr( "ERROR" ),tr( "The volume does not appear to have an entry in the wallet" ) ) ;

				this->enableAll() ;

				if( m_ui->cbKeyType->currentIndex() != keyDialog::Key ){

					m_ui->lineEditKey->setEnabled( false ) ;
				}
			}else{
				m_key = w.key ;
				this->openVolume() ;
			}
		}else{
			_internalPassWord.clear() ;
			this->enableAll() ;
		}
	}else{
		this->openVolume() ;
	}
}

void keyDialog::encfsMount()
{
	auto m = utility::mountPath( utility::mountPathPostFix( m_ui->lineEditMountPoint->text() ) ) ;

	auto ro = m_ui->checkBoxOpenReadOnly->isChecked() ;

	if( zuluMountTask::encryptedFolderMount( m_path,m,m_key,ro ).await() ){

		/*
		 * TODO: Not suspending for a while causes a process that open the mount point
		 * to exit prematurely with "QProcess: Destroyed while process is still running." warning.
		 * look into why this happens.
		 *
		 */
		this->hide() ;

		utility::Task::suspend( 2 ) ;

		m_success( m ) ;

		this->HideUI() ;
	}else{
		DialogMsg msg( this ) ;

		msg.ShowUIOK( tr( "ERROR" ),tr( "Failed to unlock an encfs/cryfs volume.\nWrong password or not an encfs/cryfs volume" ) ) ;

		if( m_ui->cbKeyType->currentIndex() == keyDialog::Key ){

			m_ui->lineEditKey->clear() ;
		}

		m_ui->lineEditKey->setFocus() ;

		return this->enableAll() ;
	}
}

void keyDialog::openVolume()
{
	auto keyType = m_ui->cbKeyType->currentIndex() ;

	if( m_volumeIsEncFs ){

		if( keyType == keyDialog::Key || keyType == keyDialog::keyKeyFile ){

			m_key = m_ui->lineEditKey->text() ;

		}else if( keyType == keyDialog::keyfile ){

			QFile f( m_ui->lineEditKey->text() ) ;

			f.open( QIODevice::ReadOnly ) ;

			m_key = f.readAll() ;

		}else if( keyType == keyDialog::plugin ){

			/*
			 * m_key is already set
			 */
		}

		return this->encfsMount() ;
	}

	if( m_ui->lineEditKey->text().isEmpty() ){

		if( keyType == keyDialog::Key || keyType == keyDialog::keyKeyFile ){

			;

		}else if( keyType == keyDialog::plugin ){

			DialogMsg msg( this ) ;
			msg.ShowUIOK( tr( "ERROR" ),tr( "Plug in name field is empty" ) ) ;

			m_ui->lineEditKey->setFocus() ;

			return this->enableAll() ;

		}else if( keyType == keyDialog::keyfile ){

			DialogMsg msg( this ) ;
			msg.ShowUIOK( tr( "ERROR" ),tr( "Keyfile field is empty" ) ) ;

			m_ui->lineEditKey->setFocus() ;

			return this->enableAll() ;
		}
	}

	auto test_name = m_ui->lineEditMountPoint->text() ;

	if( test_name.contains( "/" ) ){

		DialogMsg msg( this ) ;
		msg.ShowUIOK( tr( "ERROR" ),tr( "\"/\" character is not allowed in the mount name field" ) ) ;

		m_ui->lineEditKey->setFocus() ;

		return this->enableAll() ;
	}

	QString m ;
	if( keyType == keyDialog::Key || keyType == keyDialog::keyKeyFile ){

		auto addr = utility::keyPath() ;
		m = QString( "-f %1" ).arg( addr ) ;

		utility::keySend( addr,m_ui->lineEditKey->text() ) ;

	}else if( keyType == keyDialog::keyfile ){

		auto e = m_ui->lineEditKey->text().replace( "\"","\"\"\"" ) ;
		m = "-f \"" + utility::resolvePath( e ) + "\"" ;

	}else if( keyType == keyDialog::plugin ){

		auto r = m_ui->lineEditKey->text() ;

		if( r == "hmac" || r == "gpg" || r == "keykeyfile" ){

			if( utility::pluginKey( this,&m_key,r ) ){

				return this->enableAll() ;
			}
		}
		if( m_key.isEmpty() ){

			m = "-G " + m_ui->lineEditKey->text().replace( "\"","\"\"\"" ) ;
		}else{

			auto addr = utility::keyPath() ;
			m = QString( "-f %1" ).arg( addr ) ;

			utility::keySend( addr,m_key ) ;
		}
	}else if( keyType == keyDialog::tcryptKeys ){

		auto addr = utility::keyPath() ;
		m = QString( "-f %1 " ).arg( addr ) ;

		utility::keySend( addr,m_key ) ;
	}else{
		qDebug() << "ERROR: Uncaught condition" ;
	}

	auto volume = m_path ;
	volume.replace( "\"","\"\"\"" ) ;

	QString exe = zuluMountPath ;

	if( m_ui->checkBoxShareMountPoint->isChecked() ){

		exe += " -M -m -d \"" + volume + "\"" ;
	}else{
		exe += " -m -d \"" + volume + "\"" ;
	}

	if( m_ui->checkBoxOpenReadOnly->isChecked() ){

		exe += " -e ro" ;
	}else{
		exe += "  e rw" ;
	}

	auto mountPoint = m_ui->lineEditMountPoint->text() ;
	mountPoint.replace( "\"","\"\"\"" ) ;

	exe += " -z \"" + mountPoint + "\"" ;

	if( !m_options.isEmpty() ){

		exe += " -Y " + m_options ;
	}

	if( !m_deviceOffSet.isEmpty() ){

		exe += " -o " + m_deviceOffSet ;
	}

	if( !m_keyFiles.isEmpty() ){

		for( const auto& it : m_keyFiles ){

			auto e = it ;
			e.replace( "\"","\"\"\"" ) ;

			exe += " -F \"" + e + "\"" ;
		}
	}

	if( m_veraCryptVolume ){

		if( m_veraCryptPIMValue > 0 ){

			exe += " -t veracrypt." + QString::number( m_veraCryptPIMValue ) + " " + m ;
		}else{
			exe += " -t veracrypt " + m ;
		}
	}else{
		exe += " " + m ;
	}

	m_veraCryptWarning.show( m_veraCryptVolume ) ;

	m_working = true ;

	auto s = utility::Task::run( utility::appendUserUID( exe ) ).await() ;

	m_working = false ;

	m_veraCryptWarning.stopTimer() ;

	if( s.success() ){

		if( utility::mapperPathExists( m_path ) ) {

			/*
			 * The volume is reported as opened and it actually is
			 */

			m_success( utility::mountPath( mountPoint ) ) ;
		}else{
			/*
			 * The volume is reported as opened but it isnt,possible reason is a backe end crash
			 */

			DialogMsg msg( this ) ;

			msg.ShowUIOK( tr( "ERROR" ),tr( "An error has occured and the volume could not be opened" ) ) ;
			m_cancel() ;
		}
		this->HideUI() ;
	}else{
		m_veraCryptWarning.hide() ;

		auto keyType = m_ui->cbKeyType->currentIndex() ;

		if( s.exitCode() == 12 && keyType == keyDialog::plugin ){

			/*
			 * A user cancelled the plugin
			 */

			this->enableAll() ;
		}else{

			QString z = s.output() ;
			z.replace( tr( "ERROR: " ),"" ) ;

			DialogMsg msg( this ) ;

			msg.ShowUIOK( tr( "ERROR" ),z ) ;

			if( m_ui->cbKeyType->currentIndex() == keyDialog::Key ){

				m_ui->lineEditKey->clear() ;
			}

			this->enableAll() ;

			m_ui->lineEditKey->setFocus() ;

			if( keyType == keyDialog::keyKeyFile ){

				m_ui->cbKeyType->setCurrentIndex( 0 ) ;

				this->key() ;
			}
		}
	}
}

void keyDialog::cbActicated( int e )
{
	switch( e ){

		case keyDialog::Key        : return this->key() ;
		case keyDialog::keyfile    : return this->keyFile() ;
		case keyDialog::keyKeyFile : return this->keyAndKeyFile() ;
		case keyDialog::plugin     : return this->plugIn() ;
		case keyDialog::tcryptKeys : return this->tcryptGui() ;
	}
}

void keyDialog::keyAndKeyFile()
{
	QString key ;

	if( utility::pluginKey( this,&key,"hmac" ) ){

		m_ui->cbKeyType->setCurrentIndex( 0 ) ;
	}else{
		this->key() ;

		m_ui->lineEditKey->setEnabled( false ) ;
		m_ui->lineEditKey->setText( key ) ;
	}
}

void keyDialog::plugIn()
{
	m_ui->pbkeyOption->setIcon( QIcon( ":/module.png" ) ) ;
	m_ui->lineEditKey->setEchoMode( QLineEdit::Normal ) ;
	m_ui->label->setText( tr( "Plugin name" ) ) ;
	m_ui->pbkeyOption->setEnabled( true ) ;
	m_ui->lineEditKey->setEnabled( false ) ;
	m_ui->lineEditKey->setText( INTERNAL_WALLET ) ;
}

void keyDialog::key()
{
	m_ui->pbkeyOption->setIcon( QIcon( ":/passphrase.png" ) ) ;
	m_ui->pbkeyOption->setEnabled( false ) ;
	m_ui->label->setText( tr( "Key" ) ) ;
	m_ui->lineEditKey->setEchoMode( QLineEdit::Password ) ;
	m_ui->lineEditKey->clear() ;
	m_ui->lineEditKey->setEnabled( true ) ;
}

void keyDialog::keyFile()
{
	m_ui->pbkeyOption->setIcon( QIcon( ":/keyfile.png" ) ) ;
	m_ui->lineEditKey->setEchoMode( QLineEdit::Normal ) ;
	m_ui->label->setText( tr( "Keyfile path" ) ) ;
	m_ui->pbkeyOption->setEnabled( true ) ;
	m_ui->lineEditKey->clear() ;
	m_ui->lineEditKey->setEnabled( true ) ;
}

void keyDialog::pbCancel()
{
	this->HideUI() ;
	m_cancel() ;
}

void keyDialog::ShowUI()
{
	this->show() ;
}

void keyDialog::HideUI()
{
	if( !m_working ){

		this->hide() ;
		this->deleteLater() ;
	}
}

keyDialog::~keyDialog()
{
	m_menu->deleteLater() ;
	delete m_ui ;
}
