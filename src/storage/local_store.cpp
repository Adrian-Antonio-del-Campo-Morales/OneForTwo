/*
 * OneInTwo — Configuración, confianza y estado local persistente.
 * Estado: estructura documental; implementación pendiente.
 *
 * Responsabilidades previstas:
 * - Guardar preferencias, disposición de pantallas, pares vinculados y permisos por equipo.
 * - Guardar permisos visuales y de avisos por par, y la lista local de apps autorizadas para reenvío.
 * - Guardar el identificador del perfil elegido para cada par y validar su valor al cargar.
 * - Separar metadatos no secretos del material privado protegido por mecanismos adecuados del sistema.
 * - Escribir cambios de forma segura y validar versiones/valores al cargar.
 * - Definir rutas de usuario, cuota y metadatos de temporales con el módulo de archivos.
 * - No guardar historial de portapapeles, pulsaciones ni contenidos personales en el repositorio.
 * - Formato de configuración y mecanismo de protección de claves pendientes; ninguna credencial se crea en esta estructura.
 */

