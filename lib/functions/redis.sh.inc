#
# Forced Redis password update.
forced_redis_password_update() {
  if [ "${_REDIS_LISTEN_MODE}" = "SOCKET" ] \
    || [ "${_REDIS_LISTEN_MODE}" = "PORT" ] \
    || [ "${_REDIS_LISTEN_MODE}" = "127.0.0.1" ]; then
    if [ "${_DEBUG_MODE}" = "YES" ]; then
      msg "INFO: Generating random password for local Redis server"
    fi
    _ESC_RPASS=""
    _LEN_RPASS=0
    if [ "${_STRONG_PASSWORDS}" = "YES" ]; then
      _PWD_CHARS=32
    elif [ "${_STRONG_PASSWORDS}" = "NO" ]; then
      _PWD_CHARS=8
    else
      _STRONG_PASSWORDS=${_STRONG_PASSWORDS//[^0-9]/}
      if [ ! -z "${_STRONG_PASSWORDS}" ] \
        && [ "${_STRONG_PASSWORDS}" -gt "8" ]; then
        _PWD_CHARS="${_STRONG_PASSWORDS}"
      else
        _PWD_CHARS=8
      fi
      if [ ! -z "${_PWD_CHARS}" ] && [ "${_PWD_CHARS}" -gt "128" ]; then
        _PWD_CHARS=128
      fi
    fi
    if [ "${_STRONG_PASSWORDS}" = "YES" ] || [ "${_PWD_CHARS}" -gt "8" ]; then
      _ESC_RPASS=$(randpass "${_PWD_CHARS}" alnum 2>&1)
      _ESC_RPASS=$(echo -n "${_ESC_RPASS}" | tr -d "\n" 2>&1)
      _LEN_RPASS=$(echo ${#_ESC_RPASS} 2>&1)
    fi
    if [ -z "${_ESC_RPASS}" ] || [ "${_LEN_RPASS}" -lt "9" ]; then
      _ESC_RPASS=$(pwgen -v -s -1 2>&1)
      _ESC_RPASS=$(echo -n "${_ESC_RPASS}" | tr -d "\n" 2>&1)
      _ESC_RPASS=$(sanitize_string "${_ESC_RPASS}" 2>&1)
    fi
  else
    msg "INFO: Managing password for remote Redis server"
    if [ -e "/root/.redis.pass.txt" ] \
      && [ -e "${pthLog}/remote-redis-passwd.log" ]; then
      _ESC_RPASS=$(cat /root/.redis.pass.txt 2>&1)
      _ESC_RPASS=$(echo -n "${_ESC_RPASS}" | tr -d "\n" 2>&1)
      _ESC_RPASS=$(sanitize_string "${_ESC_RPASS}" 2>&1)
    else
      _ESC_RPASS=sCWL4tgEpyS5cLZITshxSTWRjhsUOeR6
      touch ${pthLog}/remote-redis-passwd.log
    fi
  fi
  echo "${_ESC_RPASS}" > /root/.redis.pass.txt
  chmod 0600 /root/.redis.pass.txt &> /dev/null
  touch ${pthLog}/sec-redis-pass-${_X_VERSION}-${_NOW}.log
  if [ -e "/etc/redis/redis.conf" ]; then
    _FORCE_REDIS_RESTART=YES
    sed -i "s/^# requirepass /requirepass /g" \
      /etc/redis/redis.conf &> /dev/null
    sed -i "s/^requirepass.*/requirepass ${_ESC_RPASS}/g" \
      /etc/redis/redis.conf &> /dev/null
    chown redis:redis /etc/redis/redis.conf
    chmod 0600 /etc/redis/redis.conf
  fi
}
#
# Fix Redis mode.
fix_redis_mode() {
  mkdir -p /var/run/redis
  chown redis:redis /var/run/redis
  if [ "${_CUSTOM_CONFIG_REDIS}" = "NO" ] \
    || [[ "${_THISHOST}" =~ ".host8." ]] \
    || [[ "${_THISHOST}" =~ ".boa.io" ]] \
    || [ "${_VMFAMILY}" = "VS" ]; then
    if [[ "${_THISHOST}" =~ ".host8." ]] \
      || [[ "${_THISHOST}" =~ ".boa.io" ]] \
      || [ "${_VMFAMILY}" = "VS" ]; then
      if [ -e "/root/.mstr.clstr.cnf" ] \
        || [ -e "/root/.wbhd.clstr.cnf" ] \
        || [ -e "/root/.dbhd.clstr.cnf" ] \
        || [ -e "/root/.test.clstr.cnf" ]; then
        _DO_NOTHING=YES
      else
        _REDIS_LISTEN_MODE=SOCKET
      fi
    fi
    if [ "${_REDIS_LISTEN_MODE}" = "SOCKET" ]; then
      sed -i "s/redis_client_host/redis_client_socket/g"               /data/conf/global.inc &> /dev/null
      sed -i "s/  = '127.0.0.1';/= '\/var\/run\/redis\/redis.sock';/g" /data/conf/global.inc &> /dev/null
      sed -i "s/^port 0/port 6379/g"                                   /etc/redis/redis.conf &> /dev/null
      sed -i "s/^# bind 127.0.0.1/bind 127.0.0.1/g"                    /etc/redis/redis.conf &> /dev/null
      sed -i "s/^# unixsocket/unixsocket/g"                            /etc/redis/redis.conf &> /dev/null
    elif [ "${_REDIS_LISTEN_MODE}" = "PORT" ] \
      || [ "${_REDIS_LISTEN_MODE}" = "127.0.0.1" ]; then
      _DO_NOTHING=YES
    else
      _REDIS_LISTEN_MODE=${_REDIS_LISTEN_MODE//[^0-9.]/}
      if [ ! -z "${_REDIS_LISTEN_MODE}" ]; then
        _LOCAL_REDIS_PORT_TEST=$(ifconfig \
          | grep "inet addr:${_REDIS_LISTEN_MODE} " 2>&1)
        if [[ "${_LOCAL_REDIS_PORT_TEST}" =~ "inet addr:${_REDIS_LISTEN_MODE}" ]]; then
          _REDIS_HOST=LOCAL
        else
          _REDIS_HOST=REMOTE
        fi
        if [[ "${_REDIS_LISTEN_MODE}" =~ (^)"10." ]] \
          || [[ "${_REDIS_LISTEN_MODE}" =~ (^)"192.168." ]] \
          || [[ "${_REDIS_LISTEN_MODE}" =~ (^)"172.16." ]] \
          || [[ "${_REDIS_LISTEN_MODE}" =~ (^)"127.0." ]]; then
          if [ "$_REDIS_HOST" = "LOCAL" ]; then
            sed -i "s/^bind 127.0.0.1/bind ${_REDIS_LISTEN_MODE}/g" \
              /etc/redis/redis.conf &> /dev/null
            sed -i "s/'127.0.0.1'/'${_REDIS_LISTEN_MODE}'/g" \
              /data/conf/global.inc &> /dev/null
          else
            sed -i "s/'127.0.0.1'/'${_REDIS_LISTEN_MODE}'/g" \
              /data/conf/global.inc &> /dev/null
            service redis-server stop &> /dev/null
            update-rc.d -f redis-server remove &> /dev/null
            service redis stop &> /dev/null
            update-rc.d -f redis remove &> /dev/null
            mv -f /etc/init.d/redis /etc/init.d/redis-off &> /dev/null
            mv -f /etc/init.d/redis-server \
              /etc/init.d/redis-server-off &> /dev/null
            killall -9 redis-server &> /dev/null
            rm -f /var/run/redis/redis.pid
            rm -f /var/xdrago/memcache.sh* &> /dev/null
            killall -9 memcached &> /dev/null
            msg "INFO: Remote Redis IP set to ${_REDIS_LISTEN_MODE}"
            msg "INFO: Local Redis instance has been disabled"
          fi
        else
          sed -i "s/'127.0.0.1'/'${_REDIS_LISTEN_MODE}'/g" \
            /data/conf/global.inc &> /dev/null
          service redis-server stop &> /dev/null
          update-rc.d -f redis-server remove &> /dev/null
          service redis stop &> /dev/null
          update-rc.d -f redis remove &> /dev/null
          mv -f /etc/init.d/redis /etc/init.d/redis-off &> /dev/null
          mv -f /etc/init.d/redis-server \
            /etc/init.d/redis-server-off &> /dev/null
          killall -9 redis-server &> /dev/null
          rm -f /var/run/redis/redis.pid
          rm -f /var/xdrago/memcache.sh* &> /dev/null
          killall -9 memcached &> /dev/null
          msg "INFO: Remote Redis IP set to ${_REDIS_LISTEN_MODE}"
          msg "INFO: Local Redis instance has been disabled"
        fi
      fi
    fi
  fi
}
#
# Set or update Redis password.
update_redis_password() {
  if [ -e "/etc/redis/redis.conf" ]; then
    if [ ! -e "${pthLog}/sec-redis-pass-${_X_VERSION}-${_NOW}.log" ]; then
      if [ ! -e "/root/.redis.no.new.password.cnf" ] \
        || [ ! -e "/root/.redis.pass.txt" ]; then
         forced_redis_password_update
      fi
    fi
  fi
  if [ -e "/root/.redis.pass.txt" ] && [ -e "/etc/redis/redis.conf" ]; then
    if [ -z "${_ESC_RPASS}" ]; then
      RPASS=$(cat /root/.redis.pass.txt 2>&1)
      RPASS=$(echo -n $RPASS | tr -d "\n" 2>&1)
    else
      RPASS="${_ESC_RPASS}"
    fi
    _REDIS_PWD_TEST=$(grep "'$RPASS'" /data/conf/global.inc 2>&1)
    if [[ "$_REDIS_PWD_TEST" =~ "'$RPASS'" ]]; then
      _DO_NOTHING=YES
    else
      if [ ! -z "$RPASS" ]; then
        mkdir -p /data/conf
        cp -af ${locCnf}/global.inc /data/conf/global.inc
        sed -i "s/isfoobared/$RPASS/g" /data/conf/global.inc &> /dev/null
      fi
    fi
    if [ -e "${mtrInc}" ] \
      && [ ! -L "${mtrInc}/global.inc" ] \
      && [ -e "/data/conf/global.inc" ]; then
      ln -sf /data/conf/global.inc ${mtrInc}/global.inc
    fi
    fix_redis_mode
  fi
}
#
# Install Redis from sources.
install_redis_src() {
  msg "INFO: Installing Redis update for ${_THIS_OS}/$_THIS_RV..."
  if [ ! -e "/var/lib/redis" ]; then
    st_runner "adduser --system --group redis" 2> /dev/null
  fi
  cd /var/opt
  rm -f -r redis*
  get_dev_src "redis-${_REDIS_VRN}.tar.gz"
  if [ -e "/var/lib/redis" ]; then
    mrun "service redis-server stop" &> /dev/null
    killall -9 redis-server &> /dev/null
  fi
  rm -f /usr/local/bin/redis*
  rm -f /usr/bin/redis*
  cd redis-${_REDIS_VRN}
  st_runner "make --quiet" 2> /dev/null
  st_runner "make --quiet PREFIX=/usr install" 2> /dev/null
  cp -af ${locCnf}/redis-server /etc/init.d/redis-server
  chmod 755 /etc/init.d/redis-server &> /dev/null
  st_runner "update-rc.d redis-server defaults" &> /dev/null
  mkdir -p /var/run/redis
  chown redis:redis /var/run/redis
  mkdir -p /var/log/redis
  chown redis:redis /var/log/redis
  mkdir -p /var/lib/redis
  chown redis:redis /var/lib/redis
  rm -f /var/lib/redis/*
  mkdir -p /etc/redis
  if [ -e "/etc/redis/redis.conf" ] && [ "${_STATUS}" = "UPGRADE" ]; then
    if [ "${_CUSTOM_CONFIG_REDIS}" = "NO" ] \
      || [[ "${_THISHOST}" =~ ".host8." ]] \
      || [[ "${_THISHOST}" =~ ".boa.io" ]] \
      || [ "${_VMFAMILY}" = "VS" ]; then
      if [ "$_REDIS_INSTALL_MISMATCH" = "YES" ] \
        || [ ! -e "${pthLog}/redis-${_REDIS_VRN}-${_X_VERSION}.log" ]; then
        cp -af ${locCnf}/redis.conf /etc/redis/redis.conf
      fi
    fi
  else
    if [ ! -e "/etc/redis/redis.conf" ] \
      || [ "$_REDIS_INSTALL_MISMATCH" = "YES" ] \
      || [ ! -e "${pthLog}/redis-${_REDIS_VRN}-${_X_VERSION}.log" ]; then
      cp -af ${locCnf}/redis.conf /etc/redis/redis.conf
    fi
  fi
  update_redis_password
  touch ${pthLog}/redis-${_REDIS_VRN}-${_X_VERSION}.log
}

install_update_redis() {
  if [ "${_STATUS}" = "UPGRADE" ]; then
    _REDIS_V_ITD=$(redis-server -v 2>&1 \
      | tr -d "\n" \
      | cut -d" " -f4 \
      | awk '{ print $1}' 2>&1)
    if [[ "$_REDIS_V_ITD" =~ "sha" ]]; then
      _REDIS_V_ITD=$(redis-server -v 2>&1 \
        | tr -d "\n" \
        | tr -d "v=" \
        | cut -d" " -f3 \
        | awk '{ print $1}' 2>&1)
    fi
    if [ "$_REDIS_V_ITD" = "${_REDIS_VRN}" ]; then
      _REDIS_INSTALL_MISMATCH=NO
      if [ "${_DEBUG_MODE}" = "YES" ]; then
        msg "INFO: Installed Redis version $_REDIS_V_ITD, OK"
      fi
    else
      _REDIS_INSTALL_MISMATCH=YES
      msg "INFO: Installed Redis version $_REDIS_V_ITD, upgrade required"
    fi
  else
    if [ -x "/usr/bin/redis-server" ]; then
      _REDIS_V_ITD=$(redis-server -v 2>&1 \
        | tr -d "\n" \
        | cut -d" " -f4 \
        | awk '{ print $1}' 2>&1)
      if [[ "$_REDIS_V_ITD" =~ "sha" ]]; then
        _REDIS_V_ITD=$(redis-server -v 2>&1 \
          | tr -d "\n" \
          | tr -d "v=" \
          | cut -d" " -f3 \
          | awk '{ print $1}' 2>&1)
      fi
      if [ "$_REDIS_V_ITD" = "${_REDIS_VRN}" ]; then
        _REDIS_INSTALL_MISMATCH=NO
        if [ "${_DEBUG_MODE}" = "YES" ]; then
          msg "INFO: Installed Redis version $_REDIS_V_ITD, OK"
        fi
      else
        _REDIS_INSTALL_MISMATCH=YES
        msg "INFO: Installed Redis version $_REDIS_V_ITD, rebuild required"
      fi
    fi
  fi
  if [ "$_REDIS_INSTALL_MISMATCH" = "YES" ] \
    || [ ! -e "${pthLog}/redis-${_REDIS_VRN}-${_X_VERSION}.log" ] \
    || [ ! -d "/var/run/redis" ] \
    || [ ! -x "/usr/bin/redis-server" ] \
    || [ "${_FULL_FORCE_REINSTALL}" = "YES" ]; then
    if [ "$_REDIS_HOST" = "LOCAL" ] || [ -z "$_REDIS_HOST" ]; then
      install_redis_src
    fi
  fi
}
