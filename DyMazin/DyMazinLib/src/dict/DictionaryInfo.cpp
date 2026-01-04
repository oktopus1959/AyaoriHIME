package dict

/**
 * DictionaryInfo structure
 */
class DictionaryInfo(
  /**
   * filename of dictionary
   * On Windows, filename is stored in UTF-8 encoding
   */
  var filename: String = null,

  /**
   * character set of the dictionary. e.g., "SHIFT-JIS", "UTF-8"
   */
  var charset: String = null,

  /**
   * How many words are registered in this dictionary.
   */
  var size: Int = 0,

  /**
   * dictionary type
   * this value should be USER_DIC, SYSTEM_DIC, or UNKNOWN_DIC.
   */
  var dicType: Int = 0,

  /**
   * left attributes size
   */
  var lsize: Int = 0,

  /**
   * right attributes size
   */
  var rsize: Int = 0,

  /**
   * version of this dictionary
   */
  var version: Int = 0,

  /**
   * pointer to the next dictionary info.
   */
  var next: DictionaryInfo = null
  ) extends Serializable {
}

/**
 * DictionaryInfo companion object
 */
object DictionaryInfo {
  def apply() = new DictionaryInfo()

  def apply(info: DictionaryInfo, next: DictionaryInfo) = new DictionaryInfo(
      info.filename,
      info.charset,
      info.size,
      info.dicType,
      info.lsize,
      info.rsize,
      info.version,
      next
    )

  /**
   * This is a system dictionary.
   */
  val SYSTEM_DIC = 1

  /**
   * This is a user dictionary.
   */
  val USER_DIC = 2

  /**
   * This is a unknown word dictionary.
   */
  val UNKNOWN_DIC = 3

}